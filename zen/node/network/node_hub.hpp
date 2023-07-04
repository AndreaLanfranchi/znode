/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <iostream>
#include <map>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <zen/node/common/settings.hpp>
#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/stoppable.hpp>
#include <zen/node/network/node.hpp>
#include <zen/node/network/secure.hpp>

namespace zen::network {

class NodeHub : public Stoppable {
  public:
    explicit NodeHub(NodeSettings& node_settings)
        : node_settings_{node_settings},
          io_strand_{*node_settings.asio_context},
          socket_acceptor_{*node_settings.asio_context},
          service_timer_{*node_settings.asio_context} {};

    // Not copyable or movable
    NodeHub(NodeHub& other) = delete;
    NodeHub(NodeHub&& other) = delete;
    NodeHub& operator=(const NodeHub& other) = delete;
    ~NodeHub() = default;

    [[nodiscard]] bool contains(int id) const;                     // Returns whether a node id is actually connected
    [[nodiscard]] size_t size() const;                             // Returns the number of nodes
    [[nodiscard]] std::shared_ptr<Node> operator[](int id) const;  // Returns a shared_ptr<Node> by id
    [[nodiscard]] std::vector<std::shared_ptr<Node>> get_nodes() const;  // Returns a vector of all nodes

    void start();                            // Begins accepting connections
    bool stop(bool wait) noexcept override;  // Stops accepting connections and stops all nodes

  private:
    void initialize_acceptor();  // Initialize the socket acceptor with local endpoint
    void start_accept();         // Begin async accept operation
    void handle_accept(const std::shared_ptr<Node>& new_node, const boost::system::error_code& ec);

    void on_node_disconnected(const std::shared_ptr<Node>& node);              // Handles disconnects from nodes
    void on_node_data(DataDirectionMode direction, size_t bytes_transferred);  // Handles data from nodes

    void start_service_timer();                                      // Starts the majordomo timer
    bool handle_service_timer(const boost::system::error_code& ec);  // Majordomo to serve connections
    void print_info();                                               // Prints some stats about network usage

    NodeSettings& node_settings_;  // Reference to global config settings

    std::atomic_bool is_started_{false};              // Guards against multiple starts
    boost::asio::io_context::strand io_strand_;       // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor socket_acceptor_;  // The listener socket
    boost::asio::steady_timer service_timer_;         // Service scheduler for this instance
    const uint32_t kServiceTimerIntervalSeconds_{2};  // Delay interval for service_timer_

    SSL_CTX* ssl_server_context_{nullptr};                   // For dial-in connections
    [[maybe_unused]] SSL_CTX* ssl_client_context_{nullptr};  // For dial-out connections TODO

    std::atomic_uint32_t current_active_connections_{0};
    std::atomic_uint32_t current_active_inbound_connections_{0};
    std::atomic_uint32_t current_active_outbound_connections_{0};

    std::map<int, std::shared_ptr<Node>> nodes_;  // All the connected nodes
    mutable std::mutex nodes_mutex_;              // Guards access to nodes_

    size_t total_connections_{0};
    size_t total_disconnections_{0};
    size_t total_rejected_connections_{0};
    std::atomic<size_t> total_bytes_received_{0};
    std::atomic<size_t> total_bytes_sent_{0};
    std::atomic<size_t> last_info_total_bytes_received_{0};
    std::atomic<size_t> last_info_total_bytes_sent_{0};
    StopWatch info_stopwatch_{/*auto_start=*/false};  // To measure the effective elapsed amongst two service_timer_
                                                      // events (for bandwidth calculation)
};
}  // namespace zen::network
