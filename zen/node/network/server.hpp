/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <iostream>
#include <memory>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/noncopyable.hpp>

#include <zen/node/concurrency/stoppable.hpp>
#include <zen/node/network/node.hpp>

namespace zen::network {

class TCPServer {
  public:
    TCPServer(boost::asio::io_context& io_context, SSL_CTX* ssl_context, uint16_t port, uint32_t idle_timeout_seconds,
              uint32_t max_connections);

    // Not copyable or movable
    TCPServer(TCPServer& other) = delete;
    TCPServer(TCPServer&& other) = delete;
    TCPServer& operator=(const TCPServer& other) = delete;
    ~TCPServer() = default;

    void start();
    void stop();

  private:
    void start_accept();
    void remove_node(std::shared_ptr<Node> node);

    boost::asio::io_context& io_context_;
    boost::asio::io_context::strand io_strand_;  // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor acceptor_;
    SSL_CTX* ssl_context_;
    uint32_t idle_timeout_seconds_;
    uint32_t max_connections_;
    std::atomic_uint32_t current_connections_{0};
    std::unordered_set<std::shared_ptr<Node>> nodes_;
    std::mutex nodes_mutex_;
};
}  // namespace zen::network
