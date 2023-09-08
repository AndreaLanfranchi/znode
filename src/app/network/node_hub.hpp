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
#include <core/types/network.hpp>

#include <app/common/settings.hpp>
#include <app/common/stopwatch.hpp>
#include <app/concurrency/asio_timer.hpp>
#include <app/concurrency/unique_queue.hpp>
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
          service_timer_{io_context, "NodeHub_service"},
          pending_connections_{/*capacity=*/app_settings_.network.max_active_connections} {
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

    //! \brief Returns whether the provided identifier is known and connected
    [[nodiscard]] bool contains(int node_id) const;  // Returns whether a node node_id is actually connected
    [[nodiscard]] size_t size() const;               // Returns the number of nodes
    [[nodiscard]] std::shared_ptr<Node> operator[](int node_id) const;  // Returns a shared_ptr<Node> by node_id

    [[nodiscard]] size_t bytes_sent() const noexcept { return total_bytes_sent_.load(); }
    [[nodiscard]] size_t bytes_received() const noexcept { return total_bytes_received_.load(); }

    bool start() noexcept override;          // Begins accepting connections
    bool stop(bool wait) noexcept override;  // Stops accepting connections and stops all nodes

  private:
    void initialize_acceptor();  // Initialize the socket acceptor with local endpoint
    void start_accept();         // Begin async accept operation
    void handle_accept(const boost::system::error_code& error_code,
                       boost::asio::ip::tcp::socket socket);  // Async accept handler

    void on_node_disconnected(std::shared_ptr<Node> node);  // Handles disconnects from nodes
    void on_node_data(DataDirectionMode direction,
                      size_t bytes_transferred);  // Handles data size accounting from nodes

    //! \brief Handles a message received from a node
    //! \details This function behaves as a collector of messages from nodes and will route them to the
    //! appropriate workers/handlers. Messages pertaining to node session itself MUST NOT reach here
    //! as they SHOULD be handled by the node itself.
    void on_node_received_message(std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message);

    static void set_common_socket_options(boost::asio::ip::tcp::socket& socket);  // Sets common socket options

    //! \brief Executes one maintenance cycle over all connected nodes
    //! \details Is invoked by the triggering of service_timer_ and will perform the following tasks:
    //! - Check for pending connections requests and attempt to connect to them
    //! - Check disconnected nodes and remove them from the nodes_ map
    //! - Check for nodes that have been idle for too long and disconnect them
    unsigned on_service_timer_expired(unsigned interval);

    void print_network_info();  // Prints some metric data about network usage

    void feed_connections_from_cli();  // Feed pending_connections_ from command line --network.connect
    void feed_connections_from_dns();  // Feed pending_connections_ from DNS seeds configured for chain
    std::map<std::string, std::vector<IPEndpoint>, std::less<>> dns_resolve(const std::vector<std::string>& hosts,
                                                                            const boost::asio::ip::tcp& version);
    void async_connect(const IPConnection& connection);  // Connects to a remote endpoint
    std::atomic_bool async_connecting_{false};           // Whether we are currently connecting to a remote endpoint

    AppSettings& app_settings_;              // Reference to global application settings
    boost::asio::io_context& asio_context_;  // Reference to global asio context

    boost::asio::io_context::strand asio_strand_;     // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor socket_acceptor_;  // The listener socket
    AsioTimer service_timer_;                         // Triggers a maintenance cycle

    std::unique_ptr<boost::asio::ssl::context> tls_server_context_{nullptr};  // For secure server connections
    std::unique_ptr<boost::asio::ssl::context> tls_client_context_{nullptr};  // For secure client connections

    UniqueQueue<IPConnection> pending_connections_;  // Queue of pending connections to be made (outbound)
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
