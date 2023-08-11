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
           boost::asio::ip::tcp::socket socket, boost::asio::ssl::context* ssl_context,
           std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : app_settings_(app_settings),
      connection_mode_(connection_mode),
      io_strand_(io_context),
      socket_(std::move(socket)),
      ssl_context_(ssl_context),
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {
    // TODO Set version's services according to settings
    local_version_.version = kDefaultProtocolVersion;
    local_version_.services = static_cast<uint64_t>(NetworkServicesType::kNodeNetwork);
    local_version_.timestamp = time::unix_now();
    local_version_.addr_recv.address = socket_.remote_endpoint().address();
    local_version_.addr_recv.port = socket_.remote_endpoint().port();
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
    last_message_received_time_.store(now);  // We don't want to disconnect immediately
    last_message_sent_time_.store(now);      // We don't want to disconnect immediately
    connected_time_.store(now);

    if (ssl_context_ != nullptr) {
        ssl_stream_ = std::make_unique<asio::ssl::stream<tcp::socket&>>(socket_, *ssl_context_);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_ssl_handshake(); });
    } else {
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
        push_message(abi::NetMessageType::kVersion, local_version_);
    }
}

bool Node::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopped */ {
        is_connected_.store(false);
        is_writing_.store(false);

        boost::system::error_code ec;
        if (ssl_stream_ != nullptr) {
            ssl_stream_->shutdown(ec);
            ssl_stream_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            ssl_stream_->lowest_layer().close(ec);
            // Don't reset the stream !!! There might be outstanding async operations
            // Let them gracefully complete
        } else {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        if (bool expected{true}; is_started_.compare_exchange_strong(expected, false)) {
            asio::post(io_strand_, [self{shared_from_this()}]() { self->on_disconnect_(self); });
        }
    }
    return ret;
}

void Node::start_ssl_handshake() {
    REQUIRES(ssl_context_ != nullptr);
    if (!is_connected_.load() || !socket_.is_open()) return;
    asio::ssl::stream_base::handshake_type handshake_type{connection_mode_ == NodeConnectionMode::kInbound
                                                              ? asio::ssl::stream_base::server
                                                              : asio::ssl::stream_base::client};
    ssl_stream_->set_verify_mode(asio::ssl::verify_none);
    ssl_stream_->async_handshake(handshake_type, [self{shared_from_this()}](const boost::system::error_code& ec) {
        self->handle_ssl_handshake(ec);
    });
}

void Node::handle_ssl_handshake(const boost::system::error_code& ec) {
    if (ec) {
        log::Warning("P2P node", {"id", std::to_string(node_id_), "peer", network::to_string(remote_endpoint_),
                                  "action", "SSL handshake", "error", ec.message()})
            << "Disconnecting ...";
        stop(true);
        return;
    }
    log::Trace("P2P node", {"id", std::to_string(node_id_), "peer", network::to_string(remote_endpoint_), "action",
                            "SSL handshake", "status", "success"});

    start_read();
    push_message(abi::NetMessageType::kVersion, local_version_);
}

void Node::start_read() {
    if (is_stopping()) return;
    if (ssl_stream_) {
        ssl_stream_->async_read_some(
            receive_buffer_.prepare(kMaxBytesPerIO),
            [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                self->handle_read(ec, bytes_transferred);
            });
    } else {
        // Plain unencrypted connection
        socket_.async_read_some(
            receive_buffer_.prepare(kMaxBytesPerIO),
            [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                self->handle_read(ec, bytes_transferred);
            });
    }
}

void Node::handle_read(const boost::system::error_code& ec, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (is_stopping()) return;

    if (ec) {
        log::Error("P2P node", {"id", std::to_string(node_id_), "peer", network::to_string(remote_endpoint_), "action",
                                "handle_read", "code", ec.to_string(), "error", ec.message()})
            << "Disconnecting ...";
        stop(true);
        return;
    }

    if (bytes_transferred == 0) {
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
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
    asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
}

void Node::start_write() {
    if (is_stopping()) return;
    if (bool expected{false}; !is_writing_.compare_exchange_strong(expected, true)) {
        return;  // Already writing - the queue will cause this to re-enter automatically
    }

    if (outbound_message_ && outbound_message_->data().eof()) {
        // A message has been fully sent
        outbound_message_.reset();
        outbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
    }

    if (!outbound_message_) {
        // Try to get a new message from the queue
        std::scoped_lock lock{outbound_messages_mutex_};
        if (outbound_messages_.empty()) {
            is_writing_.store(false);
            return;  // Eventually next message submission to the queue will trigger a new write cycle
        }
        outbound_message_ = std::move(outbound_messages_.front());
        outbound_message_->data().seekg(0);
        outbound_messages_.erase(outbound_messages_.begin());
        outbound_message_start_time_.store(std::chrono::steady_clock::now());
    }

    // If message has been just loaded into the barrel then we must check its validity
    // against protocol handshake rules
    if (outbound_message_->data().tellg() == 0) {
        if (app_settings_.log.log_verbosity == log::Level::kTrace) [[unlikely]] {
            log::Trace("Outbound message",
                       {"peer", this->to_string(), "command",
                        std::string(reinterpret_cast<const char*>(outbound_message_->header().command.data()))})
                << "Data : " << outbound_message_->data().to_string();
        }
        auto error{
            validate_message_for_protocol_handshake(DataDirectionMode::kOutbound, outbound_message_->get_type())};
        if (error != serialization::Error::kSuccess) {
            // TODO : Should we drop the connection here?
            // Actually outgoing messages' correct sequence is local responsibility
            // maybe we should either assert or push back the message into the queue
            log::Warning("Network message error",
                         {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_), "error",
                          std::string(magic_enum::enum_name(error))})
                << "Disconnecting peer but is local fault ...";
            outbound_message_.reset();
            is_writing_.store(false);
            stop(false);
            return;
        }
    }

    // Push remaining data from the current message to the socket
    const auto bytes_to_write{std::min(kMaxBytesPerIO, outbound_message_->data().avail())};
    const auto data{outbound_message_->data().read(bytes_to_write)};
    REQUIRES(data);
    send_buffer_.sputn(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
    if (ssl_stream_) {
        ssl_stream_->async_write_some(
            send_buffer_.data(),
            [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                self->handle_write(ec, bytes_transferred);
            });
    } else {
        socket_.async_write_some(send_buffer_.data(), [self{shared_from_this()}](const boost::system::error_code& ec,
                                                                                 const size_t bytes_transferred) {
            self->handle_write(ec, bytes_transferred);
        });
    }

    // We let handle_write to deal with re-entering the write cycle
}

void Node::handle_write(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (is_stopping()) {
        is_writing_.store(false);
        return;
    }

    if (ec) {
        log::Error("Network error", {"connection", network::to_string(remote_endpoint_), "action", "handle_write",
                                     "error", ec.message()})
            << "Disconnecting ...";
        stop(false);
        is_writing_.store(false);
        return;
    }

    if (bytes_transferred > 0) {
        bytes_sent_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kOutbound, bytes_transferred);
        send_buffer_.consume(bytes_transferred);
    }

    // If we have sent the whole message then we can start sending the next chunk
    if (send_buffer_.size()) {
        if (ssl_stream_) {
            ssl_stream_->async_write_some(
                send_buffer_.data(),
                [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                    self->handle_write(ec, bytes_transferred);
                });
        } else {
            socket_.async_write_some(
                send_buffer_.data(),
                [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                    self->handle_write(ec, bytes_transferred);
                });
        }
    } else {
        is_writing_.store(false);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
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

void Node::begin_inbound_message() {
    inbound_message_ = std::make_unique<abi::NetMessage>(version_);
    inbound_message_start_time_.store(std::chrono::steady_clock::now());
}

void Node::end_inbound_message() {
    inbound_message_.reset();
    inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
}

serialization::Error Node::parse_messages(const size_t bytes_transferred) {
    using namespace serialization;
    using enum Error;
    Error err{kSuccess};

    size_t messages_parsed{0};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (!data.empty()) {
        // We begin to time measure an inbound message from the very first byte we receive
        // This is to prevent a malicious peer from sending us a message in a SlowLoris mode
        if (!inbound_message_) begin_inbound_message();

        err = inbound_message_->parse(data);  // Consumes data
        if (err == kMessageHeaderIncomplete || is_fatal_error(err)) break;

        if (!inbound_message_->header().pristine() /* has been deserialized */) {
            if (const auto err_handshake =
                    validate_message_for_protocol_handshake(DataDirectionMode::kInbound, inbound_message_->get_type());
                err_handshake != kSuccess) {
                err = err_handshake;
                break;
            }
            //  Verify header's magic does match the network we're currently on
            if (memcmp(inbound_message_->header().network_magic.data(), app_settings_.network.magic_bytes.data(),
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

        if (app_settings_.log.log_verbosity == log::Level::kTrace) [[unlikely]] {
            log::Trace("Inbound message",
                       {"id", std::to_string(node_id_), "remote", network::to_string(remote_endpoint_), "message",
                        std::string(magic_enum::enum_name(inbound_message_->get_type())), "size",
                        to_human_bytes(inbound_message_->size())})
                << "Data: " << inbound_message_->data().to_string();
        }

        switch (inbound_message_->get_type()) {
            using enum abi::NetMessageType;
            case kVersion:
                if (err = remote_version_.deserialize(inbound_message_->data()); err != kSuccess) break;
                if (remote_version_.version < kMinSupportedProtocolVersion ||
                    remote_version_.version > kMaxSupportedProtocolVersion) {
                    log::Trace("Unsupported protocol version",
                               {"peer", this->to_string(), "version", std::to_string(version_)});
                    err = kInvalidProtocolVersion;
                    break;
                }
                version_.store(std::min(local_version_.version, remote_version_.version));
                err = push_message(abi::NetMessageType::kVerack);
                break;
            case kVerack:
                // Do nothing ... the protocol handshake status has already been updated
                // Simply create a new message container
                break;
            case kPing: {
                abi::PingPong ping_pong{};
                if (err = ping_pong.deserialize(inbound_message_->data()); err != kSuccess) break;
                err = push_message(abi::NetMessageType::kPong, ping_pong);
            } break;
            default:
                std::scoped_lock lock{inbound_messages_mutex_};
                inbound_messages_.push_back(std::move(inbound_message_));
        }

        end_inbound_message();

        //        // Protocol handshake messages can be handled directly here
        //        // Other messages are queued for processing by higher levels
        //        if (inbound_message_->get_type() == abi::NetMessageType::kVersion) {
        //            // Deserialize into local_version_ and if everything is ok
        //            // send back a verack message
        //            err = remote_version_.deserialize(inbound_message_->data());
        //            if (err == kSuccess) {
        //                if (remote_version_.version < kMinSupportedProtocolVersion ||
        //                    remote_version_.version > kMaxSupportedProtocolVersion) {
        //                    log::Trace("Unsupported protocol version",
        //                               {"peer", this->to_string(), "version", std::to_string(version_)});
        //                    err = kInvalidProtocolVersion;
        //                    break;
        //                }
        //
        //                version_.store(std::min(local_version_.version, remote_version_.version));
        //                err = push_message(abi::NetMessageType::kVerack);
        //                end_inbound_message();
        //            }
        //        } else if (inbound_message_->get_type() == abi::NetMessageType::kVerack) {
        //            // Do nothing ... the protocol handshake status has already been updated
        //            // Simply create a new message container
        //            inbound_message_ = std::make_unique<abi::NetMessage>(version_);
        //        } else {
        //            std::scoped_lock lock{inbound_messages_mutex_};
        //            inbound_messages_.push_back(std::move(inbound_message_));
        //            inbound_message_ = std::make_unique<abi::NetMessage>(version_);                   // Prepare a new
        //            container inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());  // and
        //            reset the timer
        //
        //            // TODO notify higher levels that a new message has been received
        //        }
    }
    receive_buffer_.consume(bytes_transferred);  // Discard data that has been processed
    if (!is_fatal_error(err) && messages_parsed != 0) {
        last_message_received_time_.store(std::chrono::steady_clock::now());
    }
    return err;
}

serialization::Error Node::validate_message_for_protocol_handshake(const DataDirectionMode direction,
                                                                   const abi::NetMessageType message_type) {
    using namespace zenpp::abi;
    using enum serialization::Error;

    if (protocol_handshake_status_ != ProtocolHandShakeStatus::kCompleted) [[unlikely]] {
        // Only these two guys during handshake
        // See the switch below for details about the order
        if (message_type != NetMessageType::kVersion && message_type != NetMessageType::kVerack)
            return kInvalidProtocolHandShake;

        auto status{static_cast<uint32_t>(protocol_handshake_status_.load())};
        switch (direction) {
            using enum DataDirectionMode;
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
