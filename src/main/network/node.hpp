/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <openssl/ssl.h>

#include <zen/core/abi/netmessage.hpp>
#include <zen/core/common/base.hpp>

#include <zen/node/concurrency/stoppable.hpp>
#include <zen/node/network/common.hpp>

namespace zen::network {

enum class NodeConnectionMode {
    kInbound,        // Node is accepting connections
    kOutbound,       // Node is connecting to other nodes
    kManualOutbound  // Forced connection to a specific node
};

enum class DataDirectionMode {
    kInbound,
    kOutbound
};

//! \brief Maximum number of messages to parse in a single read operation
static constexpr size_t kMaxMessagesPerRead = 32;

class Node : public Stoppable, public std::enable_shared_from_this<Node> {
  public:
    Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
         std::function<void(std::shared_ptr<Node>)> on_disconnect,
         std::function<void(DataDirectionMode, size_t)> on_data);

    Node(Node& other) = delete;
    Node(Node&& other) = delete;
    Node& operator=(const Node& other) = delete;
    ~Node() = default;

    //! \brief Start the asynchronous read/write operations
    void start();

    //! \brief Stops the asynchronous read/write operations and disconnects
    bool stop(bool wait) noexcept override;

    //! \brief Used as custom deleter for shared_ptr<Node> to ensure proper closure of the socket
    //! \attention Do not call directly, use in std::shared_ptr<Node> instead
    static void clean_up(Node* ptr) noexcept;

    enum class ProtocolHandShakeStatus : uint32_t {
        kNotInitiated = 0,
        kVersionCompleted = 1,
        kVerackCompleted = 2,
        kCompleted = 3
    };

    //! \return The unique identifier of the node
    [[nodiscard]] int id() const noexcept { return node_id_; }

    //! \brief Whether the node is inbound or outbound
    [[nodiscard]] NodeConnectionMode mode() const noexcept { return connection_mode_; }

    //! \brief Returns a reference to the underlying socket
    [[nodiscard]] boost::asio::ip::tcp::socket& socket() noexcept { return socket_; }

    //! \brief Returns whether the connection is secure
    [[nodiscard]] bool is_secure() const noexcept { return ssl_ != nullptr; }

    //! \brief Returns whether the node is currently connected
    [[nodiscard]] bool is_connected() const noexcept { return !is_stopping() && is_connected_.load(); }

    //! \brief The actual status of the protocol handshake
    [[nodiscard]] ProtocolHandShakeStatus get_protocol_handshake_status() const noexcept {
        return protocol_handshake_status_.load();
    }

    //! \brief Returns whether the node has been inactive for more than idle_timeout_seconds
    [[nodiscard]] bool is_idle(uint32_t idle_timeout_seconds) const noexcept;

    //! \brief Returns the total number of bytes read from the socket
    [[nodiscard]] size_t bytes_received() const noexcept { return bytes_received_.load(); }

    //! \brief Returns the total number of bytes written to the socket
    [[nodiscard]] size_t bytes_sent() const noexcept { return bytes_sent_.load(); }

    //! \return The string representation of the remote endpoint
    [[nodiscard]] std::string to_string() const noexcept;

    //! \return The next available node id
    [[nodiscard]] static int next_node_id() noexcept { return next_node_id_.fetch_add(1); }

  private:
    void start_ssl_handshake();
    void handle_ssl_handshake(const boost::system::error_code& ec);

    //! \brief Begin reading from the socket asychronously
    void start_read();
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    serialization::Error parse_messages(
        size_t bytes_transferred);  // Reads messages from the receive buffer and consumes buffered data

    //! \brief Finalizes the inbound message and queues it if applicable
    //! \remarks Switches from receiving mode from header to payload automatically
    [[nodiscard]] serialization::Error finalize_inbound_message();

    //! \brief Prepares for the processing of a new inbound message
    void initialize_inbound_message();

    //! \brief Returns whether the message is acceptable in the current state of the protocol handshake
    [[nodiscard]] serialization::Error validate_message_for_protocol_handshake(NetMessageType message_type);

    //! \brief Begin writing to the socket asynchronously
    void start_write();

    static std::atomic_int next_node_id_;        // Used to generate unique node ids
    const int node_id_{next_node_id()};          // Unique node id
    const NodeConnectionMode connection_mode_;   // Whether inbound or outbound
    boost::asio::io_context::strand io_strand_;  // Serialized execution of handlers
    boost::asio::ip::tcp::socket socket_;        // The underlying socket (either plain or SSL)
    boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> remote_endpoint_;  // Remote endpoint
    boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> local_endpoint_;   // Local endpoint

    SSL_CTX* ssl_context_;
    SSL* ssl_{nullptr};

    std::atomic<ProtocolHandShakeStatus> protocol_handshake_status_{
        ProtocolHandShakeStatus::kNotInitiated};                         // Status of protocol handshake
    std::atomic_int version_{0};                                         // Protocol version negotiated during handshake
    std::atomic_bool is_started_{false};                                 // Guards against multiple calls to start()
    std::atomic_bool is_connected_{true};                                // Status of socket connection
    std::atomic<std::chrono::steady_clock::time_point> connected_time_;  // Time of connection
    std::atomic<std::chrono::steady_clock::time_point> last_message_received_time_;  // Last fully "in" message tstamp
    std::atomic<std::chrono::steady_clock::time_point> last_message_sent_time_;      // Last fully "out" message tstamp
    std::function<void(std::shared_ptr<Node>)> on_disconnect_;                       // Called after stop (notifies hub)
    std::function<void(DataDirectionMode, size_t)> on_data_;  // To gather receive data stats at node hub
    boost::asio::streambuf receive_buffer_;                   // Socket receive buffer
    boost::asio::streambuf send_buffer_;                      // Socket send buffer
    std::atomic<size_t> bytes_received_{0};                   // Total bytes received from the socket during the session
    std::atomic<size_t> bytes_sent_{0};                       // Total bytes sent to the socket during the session

    bool receive_mode_header_{true};  // Whether we have switched from parsing header to accepting payload
    std::unique_ptr<NetMessageHeader> inbound_header_{nullptr};            // The header of the message being received
    std::unique_ptr<serialization::SDataStream> inbound_stream_{nullptr};  // The message data
    std::vector<std::shared_ptr<NetMessage>> inbound_messages_{};          // Queue of received messages
    std::mutex inbound_messages_mutex_{};                                  // Lock guard for received messages

    serialization::SDataStream send_stream_{serialization::Scope::kNetwork, 0};
};
}  // namespace zen::network
