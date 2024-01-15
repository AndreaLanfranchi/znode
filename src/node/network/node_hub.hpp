/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once
#include <condition_variable>
#include <list>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <core/common/misc.hpp>

#include <infra/common/settings.hpp>
#include <infra/concurrency/channel.hpp>
#include <infra/concurrency/timer.hpp>
#include <infra/network/addressbook.hpp>
#include <infra/network/traffic_meter.hpp>

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
          connector_feed_(io_context.get_executor(), settings.network.max_active_connections),
          address_book_processor_feed_(io_context.get_executor(), 500),
          address_book_{settings, io_context} {
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
    bool start() noexcept override;     // Begins accepting connections
    bool stop() noexcept override;      // Stops accepting connections and stops all nodes

    NodeService get_local_service() const;  // Returns a reference to the node service

  private:
    void initialize_acceptor();  // Initialize the socket acceptor with local endpoint

    //! \brief Handles new sockets originated either by the acceptor or by the connector
    //! and creates related nodes
    Task<void> node_factory_work();

    //! \brief Executes the dial-out connector work loop asynchronously
    Task<void> connector_work();

    //! \brief Executes the address book selector work loop asynchronously
    Task<void> address_book_selector_work();

    //! \brief Executes the address book processor work loop asynchronously
    //! \details This function will process messages targeting the address book
    Task<void> address_book_processor_work();

    //! \brief Asynchronously connects to a remoote endpoint
    Task<void> async_connect(Connection& connection);  // Connects to a remote endpoint

    //! \brief Executes the dial-in acceptor work loop asynchronously
    Task<void> acceptor_work();

    //! \brief Accounts data about node's socket connections
    void on_node_connected(std::shared_ptr<Node> node_ptr);

    //! \brief Accounts data about node's socket disconnections
    //! \remarks Requires a lock on nodes_mutex_ is holding
    void on_node_disconnected(const Node& node);

    //! \brief Handles data traffic on the wire accounting from nodes
    void on_node_data(DataDirectionMode direction,
                      size_t bytes_transferred);  // Handles data size accounting from nodes

    //! \brief Handles a message received from a node
    //! \details This function behaves as a collector of messages from nodes and will route them to the
    //! appropriate workers/handlers. Messages pertaining to node session itself MUST NOT reach here
    //! as they SHOULD be handled by the node itself. (i.e. version, verack, ping, pong)
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

    AppSettings& app_settings_;                       // Reference to global application settings
    boost::asio::io_context& asio_context_;           // Reference to global asio context
    boost::asio::io_context::strand asio_strand_;     // Serialized execution of handlers
    boost::asio::ip::tcp::acceptor socket_acceptor_;  // The listener socket
    con::Timer service_timer_;                        // Triggers a maintenance cycle
    con::Timer info_timer_;                           // Triggers printout of network info

    std::unique_ptr<boost::asio::ssl::context> tls_server_context_{nullptr};  // For secure server connections
    std::unique_ptr<boost::asio::ssl::context> tls_client_context_{nullptr};  // For secure client connections

    std::atomic_uint32_t current_active_connections_{0};
    std::atomic_uint32_t current_active_inbound_connections_{0};
    std::atomic_uint32_t current_active_outbound_connections_{0};

    con::NotifyChannel need_connections_;               // Notify channel for new connections to be established
    std::atomic_uint32_t needed_connections_count_{0};  // Number of connections to be established
    con::Channel<std::shared_ptr<Connection>> node_factory_feed_;  // Channel for new nodes to be instantiated
    con::Channel<std::shared_ptr<Connection>> connector_feed_;     // Channel for new outgoing connections

    using NodeAndPayload = std::pair<std::shared_ptr<Node>, std::shared_ptr<MessagePayload>>;
    con::Channel<NodeAndPayload> address_book_processor_feed_;  // Channel for messages targeting the address book

    net::AddressBook address_book_;                                     // The address book
    mutable std::mutex nodes_mutex_;                                    // Guards access to nodes_
    std::list<std::shared_ptr<Node>> nodes_;                            // All the connected nodes
    mutable std::mutex connected_addresses_mutex_;                      // Guards access to connected_addresses_
    std::map<boost::asio::ip::address, uint32_t> connected_addresses_;  // Addresses that are connected
    std::condition_variable all_peers_shutdown_{};                      // Used to signal shutdown of all peers
    std::mutex all_peers_shutdown_mutex_{};                             // Guards access to all_peers_shutdown_

    size_t total_connections_{0};
    size_t total_disconnections_{0};
    size_t total_rejected_connections_{0};

    net::TrafficMeter traffic_meter_{};  // Account network traffic
};
}  // namespace znode::net
