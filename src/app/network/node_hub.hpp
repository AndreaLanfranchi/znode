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

#include <core/common/misc.hpp>
#include <core/types/address.hpp>

#include <app/common/settings.hpp>
#include <app/common/stopwatch.hpp>
#include <app/concurrency/stoppable.hpp>
#include <app/network/node.hpp>
#include <app/network/secure.hpp>

namespace zenpp::network {

class NodeHub : public Stoppable {
  public:
    explicit NodeHub(AppSettings& settings, boost::asio::io_context& io_context)
        : app_settings_{settings},
          asio_context_{io_context},
          asio_strand_{io_context},
          socket_acceptor_{io_context},
          service_timer_{io_context} {
        if (app_settings_.network.nonce == 0U) {
            app_settings_.network.nonce = randomize<uint64_t>(uint64_t(/*min=*/1));
        }
    };

    // Not copyable or movable
    NodeHub(NodeHub& other) = delete;
    NodeHub(NodeHub&& other) = delete;
    NodeHub& operator=(const NodeHub& other) = delete;
    NodeHub& operator=(const NodeHub&& other) = delete;
    ~NodeHub() override = default;

    [[nodiscard]] bool contains(int node_id) const;  // Returns whether a node node_id is actually connected
    [[nodiscard]] size_t size() const;               // Returns the number of nodes
    [[nodiscard]] std::shared_ptr<Node> operator[](int node_id) const;   // Returns a shared_ptr<Node> by node_id
    [[nodiscard]] std::vector<std::shared_ptr<Node>> get_nodes() const;  // Returns a vector of all nodes

    [[nodiscard]] size_t bytes_sent() const noexcept { return total_bytes_sent_.load(); }
    [[nodiscard]] size_t bytes_received() const noexcept { return total_bytes_received_.load(); }
    [[nodiscard]] size_t active_connections_count() const noexcept { return current_active_connections_.load(); }

    bool start() noexcept override;          // Begins accepting connections
    bool stop(bool wait) noexcept override;  // Stops accepting connections and stops all nodes
    [[nodiscard]] bool connect(
        const IPEndpoint& endpoint,
        NodeConnectionMode mode = NodeConnectionMode::kOutbound);  // Connects to a remote endpoint

  private:
    void initialize_acceptor();  // Initialize the socket acceptor with local endpoint
    void start_accept();         // Begin async accept operation
    void handle_accept(const boost::system::error_code& error_code,
                       boost::asio::ip::tcp::socket socket);  // Async accept handler

    void on_node_disconnected(const std::shared_ptr<Node>& node);  // Handles disconnects from nodes
    void on_node_data(DataDirectionMode direction,
                      size_t bytes_transferred);  // Handles data size accounting from nodes
    void on_node_received_message(const std::shared_ptr<Node>& node,
                                  std::unique_ptr<abi::NetMessage>& message);  // Handles inbound messages from nodes

    static void set_common_socket_options(boost::asio::ip::tcp::socket& socket);  // Sets common socket options

    void start_service_timer();                                              // Starts the majordomo timer
    bool handle_service_timer(const boost::system::error_code& error_code);  // Majordomo to serve connections
    void print_info();                                                       // Prints some stats about network usage

    void start_connecting();  // Starts the initial dial-out connection process

    AppSettings& app_settings_;              // Reference to global application settings
    boost::asio::io_context& asio_context_;  // Reference to global asio context

    boost::asio::io_context::strand asio_strand_;     // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor socket_acceptor_;  // The listener socket
    boost::asio::steady_timer service_timer_;         // Service scheduler for this instance
    const uint32_t kServiceTimerIntervalSeconds_{2};  // Delay interval for service_timer_

    std::unique_ptr<boost::asio::ssl::context> tls_server_context_{nullptr};  // For secure connections
    std::unique_ptr<boost::asio::ssl::context> tls_client_context_{nullptr};  // For secure connections

    std::atomic_uint32_t current_active_connections_{0};
    std::atomic_uint32_t current_active_inbound_connections_{0};
    std::atomic_uint32_t current_active_outbound_connections_{0};

    std::map<int, std::shared_ptr<Node>> nodes_;                        // All the connected nodes
    std::map<boost::asio::ip::address, uint32_t> connected_addresses_;  // Addresses that are connected
    mutable std::mutex nodes_mutex_;                                    // Guards access to nodes_

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
}  // namespace zenpp::network
