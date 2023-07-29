/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <list>

#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/misc.hpp>
#include <core/common/time.hpp>

#include <app/common/log.hpp>
#include <app/network/node.hpp>
#include <app/network/secure.hpp>
#include <app/serialization/exceptions.hpp>

namespace zenpp::network {

using namespace boost;
using asio::ip::tcp;

std::atomic_int Node::next_node_id_{1};  // Start from 1 for user-friendliness

Node::Node(AppSettings& app_settings, NodeConnectionMode connection_mode, boost::asio::io_context& io_context,
           SSL_CTX* ssl_context, std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : app_settings_(app_settings),
      connection_mode_(connection_mode),
      io_strand_(io_context),
      socket_(io_context),
      ssl_context_(ssl_context),
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {
    // TODO Set version's services according to settings
    local_version_.version = kDefaultProtocolVersion;
    local_version_.services = static_cast<uint64_t>(NetworkServicesType::kNodeNetwork);
    local_version_.timestamp = time::unix_now();
    local_version_.addr_recv.address = remote_endpoint_.address();
    local_version_.addr_recv.port = remote_endpoint_.port();
    local_version_.nonce = 0;  // TODO
    local_version_.user_agent = get_buildinfo_string();
    local_version_.start_height = 0;  // TODO
    local_version_.relay = true;      // TODO
}

void Node::start() {
    if (bool expected{false}; !is_started_.compare_exchange_strong(expected, true)) {
        return;  // Already started
    }
    local_endpoint_ = socket_.local_endpoint();
    remote_endpoint_ = socket_.remote_endpoint();
    const auto now{std::chrono::steady_clock::now()};
    last_message_received_time_ = now;  // We don't want to disconnect immediately
    last_message_sent_time_ = now;      // We don't want to disconnect immediately
    connected_time_.store(now);
    inbound_message_ = std::make_unique<abi::NetMessage>(version_);

    auto self{shared_from_this()};
    if (ssl_context_ != nullptr) {
        asio::post(io_strand_, [self]() { self->start_ssl_handshake(); });
    } else {
        asio::post(io_strand_, [self]() { self->start_read(); });
    }
}

bool Node::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopped */ {
        is_connected_.store(false);
        is_writing_.store(false);

        if (ssl_ != nullptr) {
            SSL_shutdown(ssl_);    // We send our_close_notify and don't care about peer's
            SSL_set_fd(ssl_, -1);  // We don't want to close the socket (we do it manually later)
            SSL_clear(ssl_);       // Clear all data (including error state)
            SSL_free(ssl_);        // Free the SSL structure
            ssl_ = nullptr;
        }

        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);  // Shutdown both send and receive
        socket_.close(ec);
        if (bool expected{true}; is_started_.compare_exchange_strong(expected, false)) {
            auto self{shared_from_this()};
            asio::post(io_strand_, [self]() { self->on_disconnect_(self); });
        }
    }
    return ret;
}

void Node::start_ssl_handshake() {
    REQUIRES(ssl_context_ != nullptr);
    ssl_ = SSL_new(ssl_context_);

    if (!SSL_set_fd(ssl_, static_cast<int>(socket_.lowest_layer().native_handle()))) {
        auto error_code(ERR_get_error());
        print_ssl_error(error_code, log::Level::kWarning);
        stop(true);
        LOGF_TRACE << "end";
        return;
    }

    SSL_set_mode(ssl_, SSL_MODE_RELEASE_BUFFERS);
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);

    int result{0};

    // Buffer to store data
    std::array<char, 1_KiB> buffer{0};
    size_t total_bytes_read(0);
    size_t total_bytes_written(0);
    size_t bytes_read(0);
    size_t bytes_written(0);

    // Do not stay here more than 5 seconds
    auto start_handhsake_time{std::chrono::steady_clock::now()};
    while (std::chrono::steady_clock::now() - start_handhsake_time < std::chrono::seconds(5)) {
        switch (connection_mode_) {
            using enum NodeConnectionMode;
            case kInbound:
                result = SSL_accept(ssl_);
                break;
            case kOutbound:
            case kManualOutbound:
                result = SSL_connect(ssl_);
                break;
            default:
                ASSERT(false);  // Should never happen;
        }

        int ssl_error = SSL_get_error(ssl_, result);
        if (result == 1) {
            log::Info("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL handshake", "status",
                                   "Successful"});
            if (on_data_) {
                if (total_bytes_read) on_data_(DataDirectionMode::kInbound, total_bytes_read);
                if (total_bytes_written) on_data_(DataDirectionMode::kOutbound, total_bytes_written);
            }
            std::ignore = push_message(abi::NetMessageType::kVersion, local_version_);
            auto self{shared_from_this()};
            asio::post(io_strand_, [self]() { self->start_read(); });
            return;
        } else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            switch (ssl_error) {
                case SSL_ERROR_WANT_READ:
                    LOG_TRACE << "SSL_ERROR_WANT_READ";
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    SSL_read_ex(ssl_, buffer.data(), buffer.size(), &bytes_read);
                    total_bytes_read += bytes_read;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    LOG_TRACE << "SSL_ERROR_WANT_WRITE";
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    SSL_write_ex(ssl_, nullptr, 10, &bytes_written);
                    total_bytes_written += bytes_written;
                    [[fallthrough]];
                default:
                    break;
            }
        } else {
            auto error_code(ERR_get_error());
            ERR_error_string_n(error_code, buffer.data(), sizeof(buffer));
            log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL handshake", "error",
                                      std::string(buffer.data())})
                << "Disconnecting ...";
            stop(true);
            return;
        }
    }

    log::Warning("P2P node",
                 {"peer", network::to_string(remote_endpoint_), "action", "SSL handshake", "error", "Timeout reached"})
        << "Disconnecting ...";
    stop(true);
}

void Node::start_read() {
    if (!is_connected_.load() || !socket_.is_open()) return;
    LOGF_TRACE;
    auto self{shared_from_this()};
    if (!ssl_) {
        // Plain unencrypted connection
        socket_.async_read_some(receive_buffer_.prepare(64_KiB),
                                [self](const boost::system::error_code& ec, const size_t bytes_transferred) {
                                    self->handle_read(ec, bytes_transferred);
                                });
    } else {
        std::array<char, 16_KiB> buffer{0};
        size_t bytes_read{0};

        try {
            // This try block is needed cause the ssl_ pointer can be nullified during reads
            // due to a disconnection called elsewhere

            int result{SSL_read_ex(ssl_, buffer.data(), 16_KiB, &bytes_read)};
            int ssl_shutdown_status = SSL_get_shutdown(ssl_);
            if (ssl_shutdown_status & SSL_RECEIVED_SHUTDOWN) {
                log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL read", "error",
                                          "Remote sent SHUTDOWN"})
                    << "Disconnecting ...";
                stop(true);
                return;
            }

            if (result < 1) {
                int ssl_error = SSL_get_error(ssl_, result);
                if (ssl_error != SSL_ERROR_WANT_READ) {
                    // something went wrong
                    auto error_code(ERR_get_error());
                    ERR_error_string_n(error_code, buffer.data(), sizeof(buffer));
                    log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL read",
                                              "error", std::string(buffer.data())})
                        << "Disconnecting ...";
                    stop(true);
                    return;
                }
            }

            if (bytes_read > 0) {
                LOG_TRACE << "SSL_read_ex bytes read " << bytes_read;
                if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_read);
                receive_buffer_.sputn(buffer.data(), static_cast<std::streamsize>(bytes_read));
                const auto parse_result{parse_messages(bytes_read)};
                if (serialization::is_fatal_error(parse_result)) {
                    log::Warning("Network message error",
                                 {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_),
                                  "error", std::string(magic_enum::enum_name(parse_result))})
                        << "Disconnecting peer ...";
                    stop(false);
                    return;
                }
            }

            // Continue reading from socket
            if (!is_stopping()) asio::post(io_strand_, [self]() { self->start_read(); });

        } catch (...) {
            stop(true);  // Nothing more we can do
        }
    }
}

void Node::handle_read(const boost::system::error_code& ec, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (!is_connected()) return;

    if (ec) {
        log::Warning("P2P node",
                     {"peer", network::to_string(remote_endpoint_), "action", "handle_read", "error", ec.message()})
            << "Disconnecting ...";
        stop(false);
        return;
    }

    auto self{shared_from_this()};
    if (!bytes_transferred) {
        asio::post(io_strand_, [self]() { self->start_read(); });
        return;
    }

    bytes_received_ += bytes_transferred;  // This is the real traffic (including SSL overhead)
    if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_transferred);

    const auto parse_result{parse_messages(bytes_transferred)};
    if (serialization::is_fatal_error(parse_result)) {
        log::Warning("Network message error",
                     {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_), "error",
                      std::string(magic_enum::enum_name(parse_result))})
            << "Disconnecting peer ...";
        stop(false);
        return;
    }

    // Continue reading from socket
    asio::post(io_strand_, [self]() { self->start_read(); });
}

void Node::start_write() {
    if (!is_connected_.load() || !socket_.is_open()) return;
    if (bool expected{false}; !is_writing_.compare_exchange_strong(expected, true)) {
        return;  // Already writing - the queue will cause this to re-enter automatically
    }

    if (outbound_message_) {
        if (outbound_message_->data().eof()) {
            outbound_message_.reset();
            outbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
            last_message_sent_time_.store(std::chrono::steady_clock::now());
        } else {
            // Begin to time measure the message outbound time so we can quickly detect
            // slow receiving peers (due to malicious behavior or network issues)
            outbound_message_start_time_.store(std::chrono::steady_clock::now());
            if (ssl_) {
                start_write_ssl();
                return;
            } else {
                start_write_plain();
                return;
            }
        }
    }

    std::unique_lock lock{outbound_messages_mutex_};
    if (outbound_messages_.empty()) {
        lock.unlock();
        is_writing_.store(false);
        return;
    }

    // Move the first element from the queue into current message
    outbound_message_ = outbound_messages_.front();
    outbound_message_->data().seekg(0);  // Begin of message
    outbound_messages_.erase(outbound_messages_.begin());
    lock.unlock();

    auto error{validate_message_for_protocol_handshake(outbound_message_->get_type())};
    if (error != serialization::Error::kSuccess) {
        // TODO : Should we drop the connection here?
        // Actually outgoing messages' correct sequence is local responsibility
        // maybe we should either assert or push back the message into the queue

        log::Warning("Network message error",
                     {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_), "error",
                      std::string(magic_enum::enum_name(error))})
            << "Disconnecting peer ...";
        outbound_message_.reset();
        is_writing_.store(false);
        stop(false);
        return;
    }

    is_writing_.store(false);
    asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
}

void Node::start_write_ssl() {
    if (!is_connected_.load() || !socket_.is_open() || !outbound_message_ || !outbound_message_->data().avail()) {
        is_writing_.store(false);
        return;
    }

    // Read a chunk of data from the message
    auto start_time{std::chrono::steady_clock::now()};
    auto bytes_to_read{std::min(outbound_message_->data().avail(), 16_KiB)};
    auto data{outbound_message_->data().read(bytes_to_read)};
    while (!data->empty()) {
        size_t bytes_written{0};
        auto result{SSL_write_ex(ssl_, data->data(), data->size(), &bytes_written)};
        if (result < 1) {
            int ssl_error = SSL_get_error(ssl_, result);
            if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
                // something went wrong
                std::array<char, 256> buffer{0};
                auto error_code(ERR_get_error());
                ERR_error_string_n(error_code, buffer.data(), sizeof(buffer));
                log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL write", "error",
                                          std::string(buffer.data())})
                    << "Disconnecting ...";
                stop(true);
                return;
            }
        }

        if (bytes_written > 0) {
            LOG_TRACE << "SSL_write_ex bytes written " << bytes_written;
            if (on_data_) on_data_(DataDirectionMode::kOutbound, bytes_written);
            data->remove_prefix(bytes_written);
        }

        // No more than 2 seconds here - too slow
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(2)) {
            log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL write", "error",
                                      "Timeout reached"})
                << "Disconnecting ...";
            stop(true);
            return;
        }
    }

    outbound_message_->data().consume();
    is_writing_.store(false);
    asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
}

void Node::start_write_plain() {
    if (!is_connected_.load() || !socket_.is_open() || !outbound_message_ || !outbound_message_->data().avail()) {
        is_writing_.store(false);
        return;
    }

    // Read a chunk of data from the message
    auto bytes_to_read{std::min(outbound_message_->data().avail(), 16_KiB)};
    auto data{outbound_message_->data().read(bytes_to_read)};
    send_buffer_.sputn(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
    outbound_message_->data().consume();

    // Send the data collected in buffer so far
    auto self{shared_from_this()};
    socket_.async_write_some(send_buffer_.data(),
                             [self](const boost::system::error_code& ec, const size_t bytes_transferred) {
                                 self->handle_write_plain(ec, bytes_transferred);
                             });
}

void Node::handle_write_plain(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (!is_connected_.load() || !socket_.is_open()) {
        is_writing_.store(false);
        return;
    }

    auto self{shared_from_this()};
    if (ec) {
        log::Error("Network error", {"connection", network::to_string(remote_endpoint_), "action", "handle_write",
                                     "error", ec.message()})
            << "Disconnecting ...";
        boost::asio::post(io_strand_, [self]() { self->stop(false); });
        return;
    }

    if (bytes_transferred > 0) {
        bytes_sent_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kOutbound, bytes_transferred);
    }
    if (send_buffer_.size()) {
        // Resubmit the remaining data
        socket_.async_write_some(send_buffer_.data(),
                                 [self](const boost::system::error_code& ec, const size_t bytes_transferred) {
                                     self->handle_write_plain(ec, bytes_transferred);
                                 });
    } else {
        is_writing_.store(false);
        asio::post(io_strand_, [self]() { self->start_write(); });
    }
}

serialization::Error Node::push_message(const abi::NetMessageType message_type, serialization::Serializable& payload) {
    using namespace serialization;
    using enum Error;

    auto new_message = std::make_unique<abi::NetMessage>(version_);
    auto err{new_message->push(message_type, payload, app_settings_.network.magic_bytes)};
    if (!!err) {
        log::Debug("Node::push_message",
                   {"node_id", std::to_string(node_id_), "msg_type", std::string(magic_enum::enum_name(message_type)),
                    "error", std::string(magic_enum::enum_name(err))});
        return err;
    }
    std::unique_lock lock(outbound_messages_mutex_);
    outbound_messages_.emplace_back(new_message.release());
    lock.unlock();

    auto self{shared_from_this()};
    boost::asio::post(io_strand_, [self]() { self->start_write(); });
    return kSuccess;
}

serialization::Error Node::push_message(const abi::NetMessageType message_type) {
    abi::NullData null_payload{};
    return push_message(message_type, null_payload);
}

serialization::Error Node::parse_messages(const size_t bytes_transferred) {
    using namespace serialization;
    using enum Error;
    Error err{kSuccess};

    static const bool should_trace_log{log::test_verbosity(log::Level::kTrace)};

    size_t messages_parsed{0};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (!data.empty()) {
        // We begin to time measure an inbound message from the very first byte we receive
        // This is to prevent a malicious peer from sending us a message in a SlowLoris mode
        if (inbound_message_->data().empty()) {
            inbound_message_start_time_.store(std::chrono::steady_clock::now());
        }

        if (err = inbound_message_->parse(data); is_fatal_error(err)) break;
        if (err == kMessageHeaderIncomplete) break;  // Can't do anything but read other data

        if (!inbound_message_->header().pristine() /* has been deserialized */) {
            log::Trace("Network message header received",
                       {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_), "message",
                        std::string(magic_enum::enum_name(inbound_message_->get_type())), "size",
                        to_human_bytes(inbound_message_->size())});
            if (const auto err_handshake = validate_message_for_protocol_handshake(inbound_message_->get_type());
                err_handshake != kSuccess) {
                err = err_handshake;
                break;
            }
            //  Verify headers' magic matches current network we're on
            if (memcmp(inbound_message_->header().magic.data(), app_settings_.network.magic_bytes.data(),
                       app_settings_.network.magic_bytes.size()) != 0) {
                err = kMessageHeaderMagicMismatch;
                break;
            }
        }
        if (err == kMessageBodyIncomplete) break;  // Can't do anything but read other data

        // If we get here the message has been successfully parsed and can be queued for processing
        // unless we exceed the maximum number of messages per read
        // Eventually we initialize a new message and continue parsing
        if (++messages_parsed > kMaxMessagesPerRead) {
            err = KMessagesFloodingDetected;
            break;
        }

        if (should_trace_log) {
            log::Trace("Network message received",
                       {"id", std::to_string(node_id_), "remote", network::to_string(local_endpoint_), "message",
                        std::string(magic_enum::enum_name(inbound_message_->get_type())), "size",
                        to_human_bytes(inbound_message_->size())});
        }

        // Protocol handshake messages can be handled directly here
        // Other messages are queued for processing by higher levels
        if (inbound_message_->get_type() == abi::NetMessageType::kVersion) {
            // Deserialize into local_version_ and if everything is ok
            // send back a verack message
            err = remote_version_.deserialize(inbound_message_->data());
            if (err == kSuccess) {
                version_.store(std::min(local_version_.version, remote_version_.version));
                if (version_ < kMinSupportedProtocolVersion || version_ > kMaxSupportedProtocolVersion) {
                    err = kUnsupportedMessageTypeForProtocolVersion;
                    break;
                }
                err = push_message(abi::NetMessageType::kVerack);
                inbound_message_ = std::make_unique<abi::NetMessage>(version_);  // Prepare a new container
                inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());  // and reset the timer
            }
        } else if (inbound_message_->get_type() == abi::NetMessageType::kVerack) {
            // Do nothing ... the protocol handshake status has already been updated
            // Simply create a new message container
            // inbound_message_ = std::make_unique<abi::NetMessage>(version_);
        } else {
            std::unique_lock lock{inbound_messages_mutex_};
            inbound_messages_.push_back(std::move(inbound_message_));
            inbound_message_ = std::make_unique<abi::NetMessage>(version_);                   // Prepare a new container
            inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());  // and reset the timer

            // TODO notify higher levels that a new message has been received

            lock.unlock();
        }
    }
    receive_buffer_.consume(bytes_transferred);  // Discard data that has been processed
    if (!is_fatal_error(err) && messages_parsed != 0) {
        last_message_received_time_.store(std::chrono::steady_clock::now());
    }
    return err;
}

serialization::Error Node::validate_message_for_protocol_handshake(const abi::NetMessageType message_type) {
    using namespace zenpp::abi;
    using enum serialization::Error;
    if (protocol_handshake_status_ != ProtocolHandShakeStatus::kCompleted) [[unlikely]] {
        // Only these two guys during handshake
        // See the switch below for details about the order
        if (message_type != NetMessageType::kVersion && message_type != NetMessageType::kVerack)
            return kInvalidProtocolHandShake;

        auto status{static_cast<uint32_t>(protocol_handshake_status_.load())};
        switch (connection_mode_) {
            using enum NodeConnectionMode;
            using enum ProtocolHandShakeStatus;
            case kInbound:
                if (message_type == NetMessageType::kVersion &&
                    ((status & static_cast<uint32_t>(kRemoteVersionReceived)) ==
                     static_cast<uint32_t>(kRemoteVersionReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    ((status & static_cast<uint32_t>(kLocalVersionAckReceived)) ==
                     static_cast<uint32_t>(kLocalVersionAckReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                status |= static_cast<uint32_t>(message_type == NetMessageType::kVersion ? kRemoteVersionReceived
                                                                                         : kLocalVersionAckReceived);
                break;
            case kOutbound:
            case kManualOutbound:
                if (message_type == NetMessageType::kVersion &&
                    ((status & static_cast<uint32_t>(kLocalVersionSent)) == static_cast<uint32_t>(kLocalVersionSent))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    ((status & static_cast<uint32_t>(kRemoteVersionAckSent)) ==
                     static_cast<uint32_t>(kRemoteVersionAckSent))) {
                    return kDuplicateProtocolHandShake;
                }
                status |= static_cast<uint32_t>(message_type == NetMessageType::kVersion ? kLocalVersionSent
                                                                                         : kRemoteVersionAckSent);
                break;
        }
        protocol_handshake_status_.store(static_cast<ProtocolHandShakeStatus>(status));
    } else {
        switch (message_type) {
            using enum NetMessageType;
            case kVersion:  // None of these two is allowed
            case kVerack:   // after completed handshake
                return kDuplicateProtocolHandShake;
            default:
                break;
        }
    }
    return kSuccess;
}

void Node::clean_up(Node* ptr) noexcept {
    if (ptr) {
        ptr->stop(true);
        delete ptr;
    }
}

NodeIdleResult Node::is_idle() const noexcept {
    using enum NodeIdleResult;

    if (!is_connected()) return kNotIdle;  // Not connected - not idle
    using namespace std::chrono;
    const auto now{steady_clock::now()};

    // Check we've at least completed protocol handshake
    // in less than 10 seconds
    if (protocol_handshake_status_.load() != ProtocolHandShakeStatus::kCompleted) {
        if (const auto connected_time{connected_time_.load()};
            static_cast<uint32_t>(duration_cast<seconds>(now - connected_time).count()) >
            app_settings_.network.protocol_handshake_timeout_seconds) {
            return kProtocolHandshakeTimeout;
        }
    }

    // Check whether there's an inbound message in progress
    if (const auto value{inbound_message_start_time_.load()}; value != steady_clock::time_point::min()) {
        const auto inbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (inbound_message_duration > app_settings_.network.inbound_timeout_seconds) {
            return kInboundTimeout;
        }
    }

    // Check whether there's an outbound message in progress
    if (const auto value{outbound_message_start_time_.load()}; value != steady_clock::time_point::min()) {
        const auto outbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (outbound_message_duration > app_settings_.network.outbound_timeout_seconds) {
            return kOutboundTimeout;
        }
    }

    // Check whether there's been any activity
    const auto most_recent_activity_time{std::max(last_message_received_time_.load(), last_message_sent_time_.load())};
    const auto idle_seconds{duration_cast<seconds>(now - most_recent_activity_time).count()};
    if (static_cast<uint32_t>(idle_seconds) >= app_settings_.network.idle_timeout_seconds) {
        return kGlobalTimeout;
    }

    return kNotIdle;
}

std::string Node::to_string() const noexcept { return network::to_string(remote_endpoint_); }

}  // namespace zenpp::network
