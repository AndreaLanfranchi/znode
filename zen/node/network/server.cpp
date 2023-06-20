/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "server.hpp"

#include <zen/node/common/log.hpp>

namespace zen::network {

using boost::asio::ip::tcp;

TCPServer::TCPServer(boost::asio::io_context& io_context, SSL_CTX* ssl_context, uint16_t port,
                     uint32_t idle_timeout_seconds, uint32_t max_connections)
    : io_context_(io_context),
      io_strand_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      ssl_context_(ssl_context),
      idle_timeout_seconds_(idle_timeout_seconds),
      max_connections_(max_connections) {}

void TCPServer::start() { start_accept(); }

void TCPServer::stop() {
    acceptor_.close();
    std::scoped_lock lock(nodes_mutex_);
    for (auto& node : nodes_) {
        if (!node->is_connected()) continue;
        node->stop();
    }
}

void TCPServer::start_accept() {
    log::Trace("Service", {"name", "TCP Server", "status", "start_accept"});
    auto new_node =
        std::make_shared<Node>(static_cast<boost::asio::io_context&>(acceptor_.get_executor().context()), ssl_context_,
                               idle_timeout_seconds_, [this](std::shared_ptr<Node> node) { remove_node(node); });

    acceptor_.async_accept(new_node->socket(), [this, new_node](const boost::system::error_code& ec) {
        log::Trace("Service", {"name", "TCP Server", "status", "accept", "origin", "new connection"});
        if (!ec) {
            if (current_connections_ < max_connections_) {
                ++current_connections_;
                new_node->start();
                {
                    std::scoped_lock lock(nodes_mutex_);
                    nodes_.insert(new_node);
                }
                log::Info("Service", {"name", "TCP Server", "count", std::to_string(current_connections_)});
            } else {
                log::Trace("Max connections reached", {"count", std::to_string(max_connections_), "action", "reject"});
                new_node->stop();
            }
            io_strand_.post([this]() { start_accept(); });  // Continue listening for new connections

        } else if (ec == boost::asio::error::operation_aborted) {
            log::Trace("Service", {"name", "TCP Server", "status", "stop"});
        } else {
            log::Error("Service", {"name", "TCP Server", "error", ec.message()});
            io_strand_.post(
                [this]() { start_accept(); });  // Continue listening for new connections (maybe is recoverable)
        }
    });
}
void TCPServer::remove_node(std::shared_ptr<Node> node) {
    {
        std::scoped_lock lock(nodes_mutex_);
        nodes_.erase(node);
        if (current_connections_) --current_connections_;
    }
    log::Info("Service", {"name", "TCP Server", "peers", std::to_string(current_connections_)});
}

}  // namespace zen::network
