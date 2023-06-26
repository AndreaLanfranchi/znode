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

#include "zen/core/common/misc.hpp"

namespace zen::network {

Node::Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
           uint32_t idle_timeout_seconds, std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : connection_mode_(connection_mode),
      io_context_(io_context),
      io_strand_(io_context),
      socket_(io_context),
      ssl_context_(ssl_context),
      service_timer_(io_context),
      idle_timeout_seconds_{idle_timeout_seconds},
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {}

void Node::start() {
    inbound_stream_ = std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0);
    inbound_header_ = std::make_unique<NetMessageHeader>();
    if (ssl_context_ != nullptr) {
        io_strand_.post([this]() { start_ssl_handshake(); });
    } else {
        io_strand_.post([this]() {
            last_receive_time_ = std::chrono::steady_clock::now();  // We don't want to disconnect immediately
            start_service_timer();
            start_read();
        });
    }
}

void Node::stop() {
    bool expected{true};
    if (is_connected_.compare_exchange_strong(expected, false)) {
        boost::system::error_code ec;
        service_timer_.cancel(ec);
        if (ssl_ != nullptr) {
            SSL_shutdown(ssl_);  // We send our_close_notify and don't care about peer's
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        socket_.close(ec);
        io_strand_.post([this]() { on_disconnect_(shared_from_this()); });
    }
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
            start_service_timer();
            start_read();
        });
    } else {
        log::Error("Node::handle_ssl_handshake()", {"error", ec.message()});
        io_strand_.post([this]() { stop(); });
    }
}

void Node::start_service_timer() {
    if (bool expected{false}; !service_timer_started_.compare_exchange_strong(expected, true)) {
        // Timer has already been started, do nothing
        return;
    }

    service_timer_.expires_from_now(std::chrono::seconds(idle_timeout_seconds_));
    service_timer_.async_wait([this](const boost::system::error_code& ec) {
        service_timer_started_.store(false);
        if (ec == boost::asio::error::operation_aborted) {
            // Timer was cancelled, do nothing
            return;
        } else if (ec) {
            // Some error has happened with the timer log it
            // and shutdown
            log::Error("Node::start_service_timer()", {"error", ec.message()});
            return;
        }

        // Check if node has been idle too much
        auto now{std::chrono::steady_clock::now()};
        if (auto elapsed{std::chrono::duration_cast<std::chrono::seconds>(now - last_receive_time_).count()};
            static_cast<uint32_t>(elapsed) < idle_timeout_seconds_) {
            start_service_timer();
            return;
        }
        log::Warning("Node inactivity timeout", {"seconds", std::to_string(idle_timeout_seconds_)})
            << "Forcing disconnect ...";
        stop();
    });
}

void Node::start_read() {
    if (!is_connected_.load()) {
        return;
    }

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
        io_strand_.post([this_node]() { this_node->stop(); });
        return;
    }

    // Check SSL shutdown status
    if (ssl_ != nullptr) {
        int ssl_shutdown_status = SSL_get_shutdown(ssl_);
        if (ssl_shutdown_status & SSL_RECEIVED_SHUTDOWN) {
            SSL_shutdown(ssl_);
            log::Info("P2P node", {"peer", "unknown", "action", "handle_read", "message", "SSL_RECEIVED_SHUTDOWN"});
            io_strand_.post([this]() { stop(); });
            return;
        }
    }

    if (bytes_transferred > 0) {
        last_receive_time_ = std::chrono::steady_clock::now();
        bytes_received_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        if (parse_result != serialization::Error::kSuccess) {
            log::Warning("P2P message (malformed)",
                         {"peer", "unknown", "error", std::string(magic_enum::enum_name(parse_result))})
                << "Disconnecting ...";
            io_strand_.post([this]() { stop(); });
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
    serialization::Error ec{kSuccess};

    size_t messages_parsed{0};

    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (!data.empty()) {
        if (receive_mode_header_) {
            auto bytes_to_header_completion{kMessageHeaderLength - inbound_stream_->size()};
            bytes_to_header_completion = std::min(bytes_to_header_completion, data.size());
            inbound_stream_->write(data.substr(0, bytes_to_header_completion));
            data.remove_prefix(bytes_to_header_completion);

            // If header not complete yet we continue gathering
            if (inbound_stream_->size() != kMessageHeaderLength) continue;

            // Otherwise deserialize header
            if ((ec = inbound_header_->deserialize(*inbound_stream_)) != kSuccess) break;
            if ((ec = inbound_header_->validate(std::nullopt /* TODO Put network magic here */)) != kSuccess) break;
            receive_mode_header_ = false;  // Switch to body

        } else {
            auto bytes_to_body_completion{inbound_header_->length - inbound_stream_->avail()};
            bytes_to_body_completion = std::min(bytes_to_body_completion, data.size());
            inbound_stream_->write(data.substr(0, bytes_to_body_completion));
            data.remove_prefix(bytes_to_body_completion);

            // If body not complete yet we continue gathering
            if (inbound_stream_->avail() != inbound_header_->length) continue;

            // Check if body is valid (checksum verification)
            auto payload{inbound_stream_->read(inbound_stream_->avail())};
            if (!payload) {
                ec = payload.error();
                break;
            }
            crypto::Hash256 payload_digest(payload.value());
            if (auto payload_hash{payload_digest.finalize()};
                memcmp(payload_hash.data(), inbound_header_->checksum.data(),
                       sizeof(inbound_header_->checksum.size())) != 0) {
                ec = kMessageHeaderInvalidChecksum;
                break;
            }
            inbound_stream_->rewind(payload->size());  // !!! Important

            // Body and header complete - Time to start a new message
            if (log::test_verbosity(log::Level::kTrace)) {
                log::Trace("P2P message", {"peer", "unknown", "command",
                                           std::string(magic_enum::enum_name(inbound_header_->get_command())), "size",
                                           to_human_bytes(inbound_header_->length, true)});
            }

            // Protect against small messages flooding
            if (++messages_parsed > kMaxMessagesPerRead) {
                ec = KMessagesFlooding;
                break;
            }

            receive_mode_header_ = true;
            std::unique_lock lock{inbound_messages_mutex_};
            inbound_messages_.emplace_back(std::make_shared<NetMessage>(inbound_header_, inbound_stream_));
            lock.unlock();

            // TODO Notify higher level

            inbound_stream_ = std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0);
            inbound_header_ = std::make_unique<NetMessageHeader>();
        }
    }

    receive_buffer_.consume(bytes_transferred);
    return ec;
}
}  // namespace zen::network
