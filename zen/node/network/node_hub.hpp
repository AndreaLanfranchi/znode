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

#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/stoppable.hpp>
#include <zen/node/network/node.hpp>

namespace zen::network {

class NodeHub {
  public:
    NodeHub(boost::asio::io_context& io_context, SSL_CTX* ssl_context, uint16_t port, uint32_t idle_timeout_seconds,
            uint32_t max_connections);

    // Not copyable or movable
    NodeHub(NodeHub& other) = delete;
    NodeHub(NodeHub&& other) = delete;
    NodeHub& operator=(const NodeHub& other) = delete;
    ~NodeHub() = default;

    void start();
    void stop();

  private:
    void start_accept();
    void handle_accept(const std::shared_ptr<Node>& new_node, const boost::system::error_code& ec);

    void on_node_disconnected(const std::shared_ptr<Node>& node);
    void on_node_data(DataDirectionMode direction, size_t bytes_transferred);

    void start_info_timer();
    void print_info();

    boost::asio::io_context& io_context_;
    boost::asio::io_context::strand io_strand_;  // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::steady_timer info_timer_;         // Prints out stat info periodically
    static const uint32_t kInfoTimerSeconds_{10};  // Delay interval for info_timer_

    SSL_CTX* ssl_server_context_{nullptr};  // For dial-in connections
    SSL_CTX* ssl_client_context_{nullptr};  // For dial-out connections

    const uint32_t connection_idle_timeout_seconds_;
    const uint32_t max_active_connections_;
    std::atomic_uint32_t current_active_connections_{0};
    std::atomic_uint32_t current_active_inbound_connections_{0};
    std::atomic_uint32_t current_active_outbound_connections_{0};

    std::unordered_set<std::shared_ptr<Node>> nodes_;
    std::mutex nodes_mutex_;

    size_t total_connections_{0};
    size_t total_disconnections_{0};
    size_t total_rejected_connections_{0};
    std::atomic<size_t> total_bytes_received_{0};
    std::atomic<size_t> total_bytes_sent_{0};
    std::atomic<size_t> last_info_total_bytes_received_{0};
    std::atomic<size_t> last_info_total_bytes_sent_{0};
    StopWatch info_stopwatch_{/*auto_start=*/false};  // To measure the effective elapsed amongst two info_timer_ events
                                                      // (for bandwidth calculation)
};
}  // namespace zen::network
