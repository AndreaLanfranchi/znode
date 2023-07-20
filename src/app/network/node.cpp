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
    last_message_received_time_ = std::chrono::steady_clock::now();  // We don't want to disconnect immediately
    connected_time_.store(std::chrono::steady_clock::now());
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
    LOG_TRACE << "Starting SSL handshake";
    ssl_ = SSL_new(ssl_context_);
    SSL_set_fd(ssl_, static_cast<int>(socket_.lowest_layer().native_handle()));

    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_RELEASE_BUFFERS);
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);

    switch (connection_mode_) {
        case NodeConnectionMode::kInbound:
            SSL_set_accept_state(ssl_);
            break;
        case NodeConnectionMode::kOutbound:
        case NodeConnectionMode::kManualOutbound:
            SSL_set_connect_state(ssl_);
            break;
        default:
            ASSERT(false);  // Should never happen;
    }

    if (SSL_do_handshake(ssl_) != 1) {
        auto err{ERR_get_error()};
        print_ssl_error(err, log::Level::kDebug);
        stop(false);
        return;
    }

    boost::asio::async_write(socket_, boost::asio::null_buffers(),
                             [this](const boost::system::error_code& ec, std::size_t) { handle_ssl_handshake(ec); });
}

void Node::handle_ssl_handshake(const boost::system::error_code& ec) {
    auto self{shared_from_this()};
    if (!ec) {
        asio::post(i_strand_, [self]() {
            self->connected_time_.store(std::chrono::steady_clock::now());
            self->start_read();
        });
    } else {
        log::Error("Node::handle_ssl_handshake()", {"error", ec.message()});
        stop(false);
    }
}

void Node::start_read() {
    if (!is_connected_.load()) return;
    auto self{shared_from_this()};
    socket_.async_read_some(receive_buffer_.prepare(65_KiB),
                            [this, self](const boost::system::error_code& ec, const size_t bytes_transferred) {
                                handle_read(ec, bytes_transferred);
                            });
}

void Node::handle_read(const boost::system::error_code& ec, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (!is_connected()) return;

    auto self{shared_from_this()};
    if (ec) {
        log::Error("P2P node", {"peer", "unknown", "action", "handle_read", "error", ec.message()})
            << "Disconnecting ...";
        stop(false);
        return;
    }

    // Check SSL shutdown status
    if (ssl_ != nullptr) {
        int ssl_shutdown_status = SSL_get_shutdown(ssl_);
        if (ssl_shutdown_status & SSL_RECEIVED_SHUTDOWN) {
            SSL_shutdown(ssl_);
            log::Info("P2P node", {"peer", "unknown", "action", "handle_read", "message", "SSL_RECEIVED_SHUTDOWN"});
            stop(true);
            return;
        }
    }

    if (bytes_transferred > 0) {
        bytes_received_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        if (serialization::is_fatal_error(parse_result)) {
            log::Warning("Network message error",
                         {"id", std::to_string(node_id_), "remote", network::to_string(local_endpoint_), "error",
                          std::string(magic_enum::enum_name(parse_result))})
                << "Disconnecting peer ...";
            stop(false);
            return;
        }
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
        if (err = inbound_message_->parse(data); is_fatal_error(err)) break;
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
