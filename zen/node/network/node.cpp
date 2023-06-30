/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "node.hpp"

#include <magic_enum.hpp>

#include <zen/core/common/assert.hpp>
#include <zen/core/crypto/hash256.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/serialization/exceptions.hpp>

#include "zen/core/common/misc.hpp"

namespace zen::network {

Node::Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
           std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : connection_mode_(connection_mode),
      io_strand_(io_context),
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
    inbound_stream_ = std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0);
    inbound_header_ = std::make_unique<NetMessageHeader>();
    if (ssl_context_ != nullptr) {
        io_strand_.post([this]() { start_ssl_handshake(); });
    } else {
        io_strand_.post([this]() {
            last_message_received_time_ = std::chrono::steady_clock::now();  // We don't want to disconnect immediately
            connected_time_.store(std::chrono::steady_clock::now());
            start_read();
        });
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

        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);  // Shutdown both send and receive
        socket_.close(ec);
        is_connected_.store(false);
        if (bool expected{true}; is_started_.compare_exchange_strong(expected, false)) {
            io_strand_.post([this]() { on_disconnect_(shared_from_this()); });
        }
    }
    return ret;
}

void Node::start_ssl_handshake() {
    ZEN_REQUIRE(ssl_context_ != nullptr);
    ssl_ = SSL_new(ssl_context_);
    SSL_set_fd(ssl_, static_cast<int>(socket_.native_handle()));

    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_RELEASE_BUFFERS);
    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);

    SSL_set_accept_state(ssl_);
    SSL_do_handshake(ssl_);

    boost::asio::async_write(socket_, boost::asio::null_buffers(),
                             [this](const boost::system::error_code& ec, std::size_t) { handle_ssl_handshake(ec); });
}

void Node::handle_ssl_handshake(const boost::system::error_code& ec) {
    if (!ec) {
        io_strand_.post([this]() {
            connected_time_.store(std::chrono::steady_clock::now());
            start_read();
        });
    } else {
        log::Error("Node::handle_ssl_handshake()", {"error", ec.message()});
        io_strand_.post([this]() { stop(false); });
    }
}

void Node::start_read() {
    if (!is_connected_.load()) return;
    socket_.async_read_some(receive_buffer_.prepare(65_KiB),
                            [this](const boost::system::error_code& ec, const size_t bytes_transferred) {
                                handle_read(ec, bytes_transferred);
                            });
}

void Node::handle_read(const boost::system::error_code& ec, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (!is_connected()) {
        return;
    }

    if (ec) {
        log::Error("P2P node", {"peer", "unknown", "action", "handle_read", "error", ec.message()})
            << "Disconnecting ...";
        auto this_node = shared_from_this();  // Might have already been destructed elsewhere
        io_strand_.post([this_node]() { this_node->stop(false); });
        return;
    }

    // Check SSL shutdown status
    if (ssl_ != nullptr) {
        int ssl_shutdown_status = SSL_get_shutdown(ssl_);
        if (ssl_shutdown_status & SSL_RECEIVED_SHUTDOWN) {
            SSL_shutdown(ssl_);
            log::Info("P2P node", {"peer", "unknown", "action", "handle_read", "message", "SSL_RECEIVED_SHUTDOWN"});
            io_strand_.post([this]() { stop(false); });
            return;
        }
    }

    if (bytes_transferred > 0) {
        bytes_received_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        if (parse_result != serialization::Error::kSuccess) {
            log::Warning("P2P message error",
                         {"peer", "unknown", "error", std::string(magic_enum::enum_name(parse_result))})
                << "Disconnecting peer ...";
            io_strand_.post([this]() { stop(false); });
            return;
        }
    }

    // Continue reading
    io_strand_.post([this]() { start_read(); });
}

void Node::start_write() {
    // TODO implement
}

serialization::Error Node::parse_messages(size_t bytes_transferred) {
    using enum serialization::Error;

    serialization::Error err{kSuccess};
    size_t messages_parsed{0};

    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (err == kSuccess && !data.empty()) {
        auto bytes_to_section_completion{(receive_mode_header_ ? kMessageHeaderLength : inbound_header_->length) -
                                         inbound_stream_->avail()};
        bytes_to_section_completion = std::min(bytes_to_section_completion, data.size());
        inbound_stream_->write(data.substr(0, bytes_to_section_completion));
        data.remove_prefix(bytes_to_section_completion);

        const auto finalization_err{finalize_inbound_message()};
        switch (finalization_err) {
            case kSuccess:  // Message fully received and processed - queue it for processing and continue reading
                if (++messages_parsed > kMaxMessagesPerRead) {
                    err = KMessagesFlooding;
                } else {
                    std::unique_lock lock{inbound_messages_mutex_};
                    // Note ! NetMessage will take ownership of the inbound stream and header
                    inbound_messages_.emplace_back(std::make_shared<NetMessage>(inbound_header_, inbound_stream_));
                    // TODO notify higher level
                }
                inbound_stream_ = std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0);
                inbound_header_ = std::make_unique<NetMessageHeader>();
                [[fallthrough]];
            case kMessageHeaderIncomplete:  // Need more data on this header - continue reading
            case kMessageBodyIncomplete:    // Need more data on this body - continue reading
                break;
            default:
                err = finalization_err;  // Something went wrong - Report to caller and discard data
                inbound_stream_ = std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0);
                inbound_header_ = std::make_unique<NetMessageHeader>();
        }
    }

    if (err == kSuccess && messages_parsed != 0) {
        last_message_received_time_.store(std::chrono::steady_clock::now());
    }
    receive_buffer_.consume(bytes_transferred);
    return err;
}

serialization::Error Node::finalize_inbound_message() {
    using enum serialization::Error;
    serialization::Error ret{kSuccess};

    try {
        if (receive_mode_header_) {
            if (inbound_stream_->avail() != kMessageHeaderLength)
                throw serialization::SerializationException(kMessageHeaderIncomplete);
            serialization::success_or_throw(inbound_header_->deserialize(*inbound_stream_));
            serialization::success_or_throw(inbound_header_->validate(std::nullopt /* TODO Put network magic here */));
            if (protocol_handshake_status_ != ProtocolHandShakeStatus::kCompleted) {
                switch (connection_mode_) {
                    case NodeConnectionMode::kInbound:
                        if (inbound_header_->get_type() != MessageType::kVersion)
                            throw serialization::SerializationException(kInvalidProtocolHandShake);
                        break;
                    case NodeConnectionMode::kOutbound:
                    case NodeConnectionMode::kManualOutbound:
                        if (inbound_header_->get_type() != MessageType::kVerack)
                            throw serialization::SerializationException(kInvalidProtocolHandShake);
                        break;
                }
            }
            if (inbound_header_->max_payload_length().has_value() &&
                inbound_header_->max_payload_length().value() == 0) {
                serialization::success_or_throw(validate_payload_checksum(ByteView{}, inbound_header_->checksum));
                // TODO process immediately messages not requiring a deferred processing
            } else {
                ret = kMessageBodyIncomplete;  // Need body data
                receive_mode_header_ = false;  // Switch to body mode
            }
        } else {
            if (inbound_stream_->avail() != inbound_header_->length)
                throw serialization::SerializationException(kMessageBodyIncomplete);

            // Check if body is valid (checksum verification)
            const auto payload_view{inbound_stream_->read()};
            if (!payload_view) throw serialization::SerializationException(payload_view.error());
            serialization::success_or_throw(validate_payload_checksum(payload_view.value(), inbound_header_->checksum));
            inbound_stream_->rewind(payload_view->size());  // !!! Important - Return to start of body
        }

    } catch (const serialization::SerializationException& ex) {
        ret = ex.error();
    }

    return ret;
}

serialization::Error Node::validate_payload_checksum(ByteView payload, ByteView expected_checksum) noexcept {
    using enum serialization::Error;
    static crypto::Hash256 payload_digest{};
    payload_digest.init(payload);
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), expected_checksum.data(), expected_checksum.size()) != 0) {
        return kMessageHeaderInvalidChecksum;
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
    ZEN_REQUIRE(idle_timeout_seconds != 0);
    std::chrono::seconds::rep idle_seconds{0};
    const auto now{std::chrono::steady_clock::now()};
    const auto last_message_received_time{last_message_received_time_.load()};
    const auto last_message_sent_time{last_message_sent_time_.load()};

    if (last_message_received_time == std::chrono::steady_clock::time_point::min() &&
        last_message_sent_time == std::chrono::steady_clock::time_point::min()) {
        idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - connected_time_.load()).count();
    } else {
        idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                           now - std::max(last_message_received_time, last_message_sent_time))
                           .count();
    }
    return (static_cast<uint32_t>(idle_seconds) >= idle_timeout_seconds);
}

std::string Node::to_string() const noexcept { return network::to_string(remote_endpoint_); }

}  // namespace zen::network
