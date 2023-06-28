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
static constexpr size_t kMaxMessagesPerRead = 100;

class Node : public Stoppable, public std::enable_shared_from_this<Node> {
  public:
    Node(NodeConnectionMode connection_mode, boost::asio::io_context& io_context, SSL_CTX* ssl_context,
         uint32_t idle_timeout_seconds, std::function<void(std::shared_ptr<Node>)> on_disconnect,
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

    //! \brief Whether the node is inbound or outbound
    [[nodiscard]] NodeConnectionMode mode() const noexcept { return connection_mode_; }

    //! \brief Returns a reference to the underlying socket
    [[nodiscard]] boost::asio::ip::tcp::socket& socket() noexcept { return socket_; }

    //! \brief Returns whether the connection is secure
    [[nodiscard]] bool is_secure() const noexcept { return ssl_ != nullptr; }

    //! \brief Returns whether the node is currently connected
    [[nodiscard]] bool is_connected() const noexcept { return !is_stopping() && is_connected_.load(); }

    //! \brief Returns the total number of bytes read from the socket
    [[nodiscard]] size_t bytes_received() const noexcept { return bytes_received_.load(); }

    //! \brief Returns the total number of bytes written to the socket
    [[nodiscard]] size_t bytes_sent() const noexcept { return bytes_sent_.load(); }

  private:
    void start_ssl_handshake();
    void handle_ssl_handshake(const boost::system::error_code& ec);

    //! \brief Start maintenance timer
    void start_service_timer();

    //! \brief Begin reading from the socket asychronously
    void start_read();
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    serialization::Error parse_messages(
        size_t bytes_transferred);  // Reads messages from the receive buffer and consumes buffered data

    //! \brief Begin writing to the socket asychronously
    void start_write();

    const NodeConnectionMode connection_mode_;
    boost::asio::io_context::strand io_strand_;  // Serialized execution of handlers
    boost::asio::ip::tcp::socket socket_;
    SSL_CTX* ssl_context_;
    SSL* ssl_{nullptr};
    boost::asio::steady_timer service_timer_;
    std::atomic_bool service_timer_started_{false};
    uint32_t idle_timeout_seconds_;

    std::function<void(std::shared_ptr<Node>)> on_disconnect_;
    std::function<void(DataDirectionMode, size_t)> on_data_;  // To gather receive data stats at higher level

    std::atomic_bool is_started_{false}; // Guards against multiple calls to start()
    std::atomic_bool is_connected_{true};

    boost::asio::streambuf receive_buffer_;  // Socket receive buffer
    boost::asio::streambuf send_buffer_;     // Socket send buffer

    std::chrono::steady_clock::time_point last_receive_time_;
    std::chrono::steady_clock::time_point last_send_time_;

    serialization::SDataStream send_stream_{serialization::Scope::kNetwork, 0};

    std::atomic<size_t> bytes_received_{0};
    std::atomic<size_t> bytes_sent_{0};

    bool receive_mode_header_{true};  // Whether we have switched from parsing header to accepting payload
    std::unique_ptr<NetMessageHeader> inbound_header_{nullptr};            // The header of the message being received
    std::unique_ptr<serialization::SDataStream> inbound_stream_{nullptr};  // The message data
    std::vector<std::shared_ptr<NetMessage>> inbound_messages_{};          // Queue of received messages
    std::mutex inbound_messages_mutex_{};                                  // Lock guard for received messages
};
}  // namespace zen::network
