/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "server.hpp"

#include <zen/core/common/misc.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/network/common.hpp>

namespace zen::network {

using boost::asio::ip::tcp;

TCPServer::TCPServer(boost::asio::io_context& io_context, SSL_CTX* ssl_context, uint16_t port,
                     uint32_t idle_timeout_seconds, uint32_t max_connections)
    : io_context_(io_context),
      io_strand_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      info_timer_(io_context),
      ssl_context_(ssl_context),
      connection_idle_timeout_seconds_(idle_timeout_seconds),
      max_active_connections_(max_connections) {}

void TCPServer::start() {
    start_info_timer();
    start_accept();
}

void TCPServer::start_info_timer() {
    info_timer_.expires_after(std::chrono::seconds(kInfoTimerSeconds_));
    info_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;  // No other operation allowed
        } else if (ec) {
            log::Error("Service", {"name", "TCP Server", "action", "info_timer_tick", "error", ec.message()});
            // Fall through and resubmit
        } else if (!ec) {
            print_info();
        }
        start_info_timer();
    });
}

void TCPServer::print_info() {
    std::vector<std::string> info_data;

    auto current_total_bytes_received{total_bytes_received_.load()};
    auto current_total_bytes_sent{total_bytes_sent_.load()};
    auto period_total_bytes_received{current_total_bytes_received - last_info_total_bytes_received_.load()};
    auto period_total_bytes_sent{current_total_bytes_sent - last_info_total_bytes_sent_.load()};

    info_data.insert(info_data.end(), {"peers i/o", std::to_string(current_active_inbound_connections_.load()) + "/" +
                                                        std::to_string(current_active_outbound_connections_.load())});
    info_data.insert(info_data.end(), {"data i/o", to_human_bytes(current_total_bytes_received, true) + " " +
                                                       to_human_bytes(current_total_bytes_sent, true)});

    auto period_bytes_received_per_second{to_human_bytes(period_total_bytes_received / kInfoTimerSeconds_, true) +
                                          "s"};
    auto period_bytes_sent_per_second{to_human_bytes(period_total_bytes_sent / kInfoTimerSeconds_, true) + "s"};

    info_data.insert(info_data.end(),
                     {"speed i/o", period_bytes_received_per_second + " " + period_bytes_sent_per_second});

    last_info_total_bytes_received_.store(current_total_bytes_received);
    last_info_total_bytes_sent_.store(current_total_bytes_sent);

    log::Info("Network usage", info_data);
}

void TCPServer::stop() {
    info_timer_.cancel();
    acceptor_.close();
    std::scoped_lock lock(nodes_mutex_);
    for (auto& node : nodes_) {
        if (!node->is_connected()) continue;
        node->stop();
    }
}

void TCPServer::start_accept() {
    log::Trace("Service", {"name", "TCP Server", "status", "Listening"});
    auto new_node = std::make_shared<Node>(
        NodeConnectionMode::kInbound, io_context_, ssl_context_, connection_idle_timeout_seconds_,
        [this](std::shared_ptr<Node> node) { on_node_disconnected(node); },
        [this](DataDirectionMode direction, size_t bytes_transferred) { on_node_data(direction, bytes_transferred); });

    acceptor_.async_accept(new_node->socket(),
                           [this, new_node](const boost::system::error_code& ec) { handle_accept(new_node, ec); });
}

void TCPServer::handle_accept(std::shared_ptr<Node> new_node, const boost::system::error_code& ec) {
    std::string origin{"unknown"};
    if (auto remote_endpoint{get_remote_endpoint(new_node->socket())}; remote_endpoint) {
        origin = to_string(remote_endpoint.value());
    }

    log::Trace("Service", {"name", "TCP Server", "status", "handle_accept", "origin", origin});

    if (ec == boost::asio::error::operation_aborted) {
        log::Trace("Service", {"name", "TCP Server", "status", "stop"});
        return;  // No other operation allowed
    } else if (ec) {
        log::Error("Service", {"name", "TCP Server", "error", ec.message()});
        // Fall through and continue listening for new connections
    } else if (!ec) {
        ++total_connections_;

        // Check we do not exceed the maximum number of connections
        if (current_active_connections_ >= max_active_connections_) {
            log::Trace("Service",
                       {"name", "TCP Server", "peers", std::to_string(max_active_connections_), "action", "reject"});
            new_node->stop();
            ++total_rejected_connections_;
            return;
        }

        ++current_active_connections_;
        ++current_active_inbound_connections_;
        new_node->start();
        std::scoped_lock lock(nodes_mutex_);
        nodes_.insert(new_node);
        log::Info("Service",
                  {"name", "TCP Server", "new peer", origin, "peers", std::to_string(current_active_connections_)});
        // Fall through and continue listening for new connections
    }

    io_strand_.post([this]() { start_accept(); });  // Continue listening for new connections
}

void TCPServer::on_node_disconnected(std::shared_ptr<Node> node) {
    std::scoped_lock lock(nodes_mutex_);
    nodes_.erase(node);
    if (current_active_connections_) --current_active_connections_;

    ++total_disconnections_;
}

void TCPServer::on_node_data(zen::network::DataDirectionMode direction, const size_t bytes_transferred) {
    switch (direction) {
        case DataDirectionMode::kInbound:
            total_bytes_received_ += bytes_transferred;
            break;
        case DataDirectionMode::kOutbound:
            total_bytes_sent_ += bytes_transferred;
            break;
    }
}
}  // namespace zen::network
