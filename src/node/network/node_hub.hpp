/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <iostream>
#include <list>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <core/common/misc.hpp>

#include <infra/common/random.hpp>
#include <infra/common/stopwatch.hpp>
#include <infra/concurrency/channel.hpp>
#include <infra/concurrency/timer.hpp>
#include <infra/concurrency/unique_queue.hpp>
#include <infra/network/traffic_meter.hpp>

#include <node/common/settings.hpp>
#include <node/network/connection.hpp>
#include <node/network/node.hpp>
#include <node/network/secure.hpp>

namespace znode::net {

class NodeHub : public con::Stoppable {
  public:
    explicit NodeHub(AppSettings& settings, boost::asio::io_context& io_context)
        : app_settings_{settings},
          asio_context_{io_context},
          asio_strand_{io_context},
          socket_acceptor_{io_context},
          service_timer_{io_context, "nh_service", true},
          info_timer_{io_context, "nh_info", true},
          need_connections_(io_context.get_executor()),
          node_factory_feed_(io_context.get_executor(), settings.network.max_active_connections),
          connector_feed_(io_context.get_executor(), settings.network.max_active_connections) {
        if (app_settings_.network.nonce == 0U) {
            app_settings_.network.nonce = randomize<uint64_t>(/*min=*/1U);
        }
    };

    // Not copyable or movable
    NodeHub(NodeHub& other) = delete;
    NodeHub(NodeHub&& other) = delete;
    NodeHub& operator=(const NodeHub& other) = delete;
    NodeHub& operator=(const NodeHub&& other) = delete;
    ~NodeHub() override = default;

    [[nodiscard]] size_t size() const;  // Returns the number of nodes

    bool start() noexcept override;  // Begins accepting connections
    bool stop() noexcept override;   // Stops accepting connections and stops all nodes

  private:
    void initialize_acceptor();  // Initialize the socket acceptor with local endpoint

    //! \brief Handles new sockets originated either by the acceptor or by the connector
    //! and creates related nodes
    Task<void> node_factory_work();

    //! \brief Executes the connector work loop asynchronously
    Task<void> connector_work();

    //! \brief Asynchronously connects to a remoote endpoint
    Task<void> async_connect(Connection& connection);  // Connects to a remote endpoint

    //! \brief Executes the acceptor work loop asynchronously
    Task<void> acceptor_work();

    //! \brief Accounts data about node's socket disconnections
    //! \remarks Requires a lock on nodes_mutex_ is holding
    void on_node_disconnected(const std::shared_ptr<Node>& node);

    //! \brief Accounts data about node's socket connections
    void on_node_connected(const std::shared_ptr<Node>& node);

    //! \brief Handles data traffic on the wire accounting from nodes
    void on_node_data(DataDirectionMode direction,
                      size_t bytes_transferred);  // Handles data size accounting from nodes

    //! \brief Handles a message received from a node
    //! \details This function behaves as a collector of messages from nodes and will route them to the
    //! appropriate workers/handlers. Messages pertaining to node session itself MUST NOT reach here
    //! as they SHOULD be handled by the node itself.
    void on_node_received_message(std::shared_ptr<Node> node_ptr, std::shared_ptr<MessagePayload> payload_ptr);

    static void set_common_socket_options(boost::asio::ip::tcp::socket& socket);  // Sets common socket options

    //! \brief Executes one maintenance cycle over all connected nodes
    //! \details Is invoked by the triggering of service_timer_ and will perform the following tasks:
    //! - Check for pending connections requests and attempt to connect to them
    //! - Check disconnected nodes and remove them from the nodes_ map
    //! - Check for nodes that have been idle for too long and disconnect them
    void on_service_timer_expired(con::Timer::duration& interval);

    //! \brief Periodically prints some metric data about network usage
    void on_info_timer_expired(con::Timer::duration& interval);

    void feed_connections_from_cli();  // Feed node_factory_feed_ from command line --network.connect
    void feed_connections_from_dns();  // Feed node_factory_feed_ from DNS seeds configured for chain
    std::map<std::string, std::vector<IPEndpoint>, std::less<>> dns_resolve(const std::vector<std::string>& hosts,
                                                                            const boost::asio::ip::tcp& version);

    AppSettings& app_settings_;              // Reference to global application settings
    boost::asio::io_context& asio_context_;  // Reference to global asio context

    boost::asio::io_context::strand asio_strand_;     // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor socket_acceptor_;  // The listener socket
    con::Timer service_timer_;                        // Triggers a maintenance cycle
    con::Timer info_timer_;                           // Triggers printout of network info

    std::unique_ptr<boost::asio::ssl::context> tls_server_context_{nullptr};  // For secure server connections
    std::unique_ptr<boost::asio::ssl::context> tls_client_context_{nullptr};  // For secure client connections

    std::atomic_uint32_t current_active_connections_{0};
    std::atomic_uint32_t current_active_inbound_connections_{0};
    std::atomic_uint32_t current_active_outbound_connections_{0};

    con::NotifyChannel need_connections_;  // Notify channel for new connections to be established
    con::Channel<std::shared_ptr<Connection>> node_factory_feed_;  // Channel for new nodes to be instantiated
    con::Channel<std::shared_ptr<Connection>> connector_feed_;     // Channel for new outgoing connections

    std::list<std::shared_ptr<Node>> nodes_;                            // All the connected nodes
    std::map<boost::asio::ip::address, uint32_t> connected_addresses_;  // Addresses that are connected
    mutable std::mutex nodes_mutex_;                                    // Guards access to nodes_

    size_t total_connections_{0};
    size_t total_disconnections_{0};
    size_t total_rejected_connections_{0};

    net::TrafficMeter traffic_meter_{};  // Account network traffic
};
}  // namespace znode::net
