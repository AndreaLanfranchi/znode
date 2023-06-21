/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "node.hpp"

#include <zen/node/common/log.hpp>

namespace zen::network {

Node::Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
           uint32_t idle_timeout_seconds, std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : connection_mode_(connection_mode),
      io_context_(io_context),
      io_strand_(io_context),
      socket_(io_context),
      ssl_context_(ssl_context),
      idle_timer_(io_context),
      idle_timeout_seconds_{idle_timeout_seconds},
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {}

void Node::start() {
    if (ssl_context_ != nullptr) {
        io_strand_.post([this]() { start_ssl_handshake(); });
    } else {
        io_strand_.post([this]() {
            start_idle_timer();
            start_read();
        });
    }
}

void Node::stop() {
    bool expected{true};
    if (is_connected_.compare_exchange_strong(expected, false)) {
        boost::system::error_code ec;
        idle_timer_.cancel(ec);
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
            start_idle_timer();
            start_read();
        });
    } else {
        log::Error("Node::handle_ssl_handshake()", {"error", ec.message()});
        io_strand_.post([this]() { stop(); });
    }
}

void Node::start_idle_timer() {
    if (bool expected{false}; !idle_timer_started_.compare_exchange_strong(expected, true)) {
        // Timer has already been started, do nothing
        return;
    }

    idle_timer_.expires_from_now(std::chrono::seconds(idle_timeout_seconds_));
    idle_timer_.async_wait([this](const boost::system::error_code& ec) {
        idle_timer_started_.store(false);
        if (ec == boost::asio::error::operation_aborted) {
            // Timer was cancelled, do nothing
            return;
        } else if (ec) {
            // Some error has happened with the timer log it
            // and shutdown
            log::Error("Node::start_idle_timer()", {"error", ec.message()});
            return;
        }
        // Timer has ticked, disconnect
        // TODO check effective idle timeout
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

    if (!ec) {
        // Check SSL shutdown status
        if (ssl_ != nullptr) {
            int ssl_shutdown_status = SSL_get_shutdown(ssl_);
            if (ssl_shutdown_status & SSL_RECEIVED_SHUTDOWN) {
                SSL_shutdown(ssl_);
                log::Trace("Node::handle_read()", {"message", "SSL_RECEIVED_SHUTDOWN"});
                io_strand_.post([this]() { stop(); });
                return;
            }
        }
        if (bytes_transferred > 0) {
            bytes_received_ += bytes_transferred;
            if (on_data_) on_data_(DataDirectionMode::kInbound, bytes_transferred);

            // TODO implement reading into messages

            receive_buffer_.consume(bytes_transferred);
        }
        io_strand_.post([this]() { start_read(); });
    } else {
        log::Error("Node::handle_read()", {"error", ec.message()});
        auto this_node = shared_from_this();  // Might have already been destructed elsewhere
        io_strand_.post([this_node]() { this_node->stop(); });
    }
}

void Node::start_write() {
    // TODO implement
}

}  // namespace zen::network
