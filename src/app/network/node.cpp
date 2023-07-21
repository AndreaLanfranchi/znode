/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <list>

#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/misc.hpp>

#include <app/common/log.hpp>
#include <app/network/node.hpp>
#include <app/network/secure.hpp>
#include <app/serialization/exceptions.hpp>

namespace zenpp::network {

using namespace boost;
using asio::ip::tcp;

std::atomic_int Node::next_node_id_{1};  // Start from 1 for user-friendliness

Node::Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
           std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : connection_mode_(connection_mode),
      i_strand_(io_context),
      o_strand_(io_context),
      socket_(io_context),
      ssl_context_(ssl_context),
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {}

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
        asio::post(i_strand_, [self]() { self->start_ssl_handshake(); });
    } else {
        asio::post(i_strand_, [self]() { self->start_read(); });
    }
}

bool Node::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopped */ {
        if (ssl_ != nullptr) {
            SSL_shutdown(ssl_);    // We send our_close_notify and don't care about peer's
            SSL_set_fd(ssl_, -1);  // We don't want to close the socket (we do it manually later)
            SSL_clear(ssl_);       // Clear all data (including error state)
            SSL_free(ssl_);        // Free the SSL structure
            ssl_ = nullptr;
        }

        is_connected_.store(false);
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);  // Shutdown both send and receive
        socket_.close(ec);
        if (bool expected{true}; is_started_.compare_exchange_strong(expected, false)) {
            auto self{shared_from_this()};
            asio::post(i_strand_, [self]() { self->on_disconnect_(self); });
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

    // SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(ssl_, SSL_MODE_RELEASE_BUFFERS);
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);

    // Tries the operation for at most 5 seconds

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
            auto self{shared_from_this()};
            asio::post(i_strand_, [self]() { self->start_read(); });
            // TODO post message advertising local version
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
                log::Warning("P2P node", {"peer", network::to_string(remote_endpoint_), "action", "SSL read", "error",
                                          std::string(buffer.data())})
                    << "Disconnecting ...";
                stop(true);
                return;
            }
        }

        if (bytes_read > 0) {
            LOG_TRACE << "SSL_read_ex bytes read " << bytes_read;
            if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_read);
            receive_buffer_.sputn(buffer.data(), static_cast<std::streamsize>(bytes_read));
        }

        // Consume the data
        asio::post(i_strand_, [self]() { self->start_read(); });
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
        asio::post(i_strand_, [self]() { self->start_read(); });
        return;
    }

    bytes_received_ += bytes_transferred;  // This is the real traffic (including SSL overhead)
    if (on_data_ && bytes_transferred) on_data_(DataDirectionMode::kInbound, bytes_transferred);

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
    asio::post(i_strand_, [self]() { self->start_read(); });
}

void Node::start_write() {
    // TODO implement
}

serialization::Error Node::parse_messages(const size_t bytes_transferred) {
    using namespace serialization;
    using enum Error;
    Error err{kSuccess};

    static const bool should_trace_log{log::test_verbosity(log::Level::kTrace)};

    size_t messages_parsed{0};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (!data.empty()) {
        if (err = inbound_message_->parse(data); is_fatal_error(err)) {
            if (should_trace_log) {
                LOG_TRACE << "Data: " << inbound_message_->data().to_string();
                LOG_TRACE << "P Length: " << inbound_message_->header().length;
            }
            break;
        }
        if (err == kMessageHeaderIncomplete) break;  // Can't do anything but read other data
        if (!inbound_message_->header().pristine() /* has been deserialized */) {
            if (const auto err_handshake = validate_message_for_protocol_handshake(inbound_message_->get_type());
                err_handshake != kSuccess) {
                err = err_handshake;
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

        // TODO verify this message is actually intended for the active network
        // TODO by matching network config magic bytes

        if (should_trace_log) {
            log::Trace("Network message received",
                       {"id", std::to_string(node_id_), "remote", network::to_string(local_endpoint_), "message",
                        std::string(magic_enum::enum_name(inbound_message_->get_type())), "size",
                        to_human_bytes(inbound_message_->size())});
        }

        std::unique_lock lock{inbound_messages_mutex_};
        inbound_messages_.push_back(std::move(inbound_message_));
        inbound_message_ = std::make_unique<abi::NetMessage>(version_);  // Start with a brand new message
        // TODO notify higher levels that a new message has been received
        lock.unlock();
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
                    (status & static_cast<uint32_t>(kRemoteVersionReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    (status & static_cast<uint32_t>(kLocalVersionAckReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                status |= static_cast<uint32_t>(message_type == NetMessageType::kVersion ? kRemoteVersionReceived
                                                                                         : kLocalVersionAckReceived);
                break;
            case kOutbound:
            case kManualOutbound:
                if (message_type == NetMessageType::kVersion && (status & static_cast<uint32_t>(kLocalVersionSent))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    (status & static_cast<uint32_t>(kRemoteVersionAckSent))) {
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

bool Node::is_idle(const uint32_t idle_timeout_seconds) const noexcept {
    REQUIRES(idle_timeout_seconds != 0);
    if (!is_connected()) return false;  // Not connected - not idle
    using namespace std::chrono;
    const auto now{steady_clock::now()};
    const auto most_recent_activity_time{std::max(last_message_received_time_.load(), last_message_sent_time_.load())};
    const auto idle_seconds{duration_cast<seconds>(now - most_recent_activity_time).count()};
    return (static_cast<uint32_t>(idle_seconds) >= idle_timeout_seconds);
}

std::string Node::to_string() const noexcept { return network::to_string(remote_endpoint_); }

}  // namespace zenpp::network
