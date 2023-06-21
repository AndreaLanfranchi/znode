/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <openssl/ssl.h>

namespace zen::network {

enum class NodeConnectionMode {
    kInbound,
    kOutbound
};

enum class DataDirectionMode {
    kInbound,
    kOutbound
};

class Node : public std::enable_shared_from_this<Node> {
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
    void stop();

    //! \brief Whether the node is inbound or outbound
    [[nodiscard]] NodeConnectionMode mode() const noexcept { return connection_mode_; }

    //! \brief Returns a reference to the underlying socket
    [[nodiscard]] boost::asio::ip::tcp::socket& socket() noexcept { return socket_; }

    //! \brief Returns whether the connection is secure
    [[nodiscard]] bool is_secure() const noexcept { return ssl_ != nullptr; }

    //! \brief Returns whether the node is currently connected
    [[nodiscard]] bool is_connected() const noexcept { return is_connected_.load(); }

    //! \brief Returns the total number of bytes read from the socket
    [[nodiscard]] size_t bytes_received() const noexcept { return bytes_received_.load(); }

    //! \brief Returns the total number of bytes written to the socket
    [[nodiscard]] size_t bytes_sent() const noexcept { return bytes_sent_.load(); }

  private:
    void start_ssl_handshake();
    void handle_ssl_handshake(const boost::system::error_code& ec);

    //! \brief Start measuring idle time
    void start_idle_timer();

    //! \brief Begin reading from the socket asychronously
    void start_read();
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);

    //! \brief Begin writing to the socket asychronously
    void start_write();

    const NodeConnectionMode connection_mode_;
    boost::asio::io_context& io_context_;
    boost::asio::io_context::strand io_strand_;  // Serialized execution of handlers
    boost::asio::ip::tcp::socket socket_;
    SSL_CTX* ssl_context_;
    SSL* ssl_{nullptr};
    boost::asio::steady_timer idle_timer_;
    std::atomic_bool idle_timer_started_{false};
    uint32_t idle_timeout_seconds_;

    std::function<void(std::shared_ptr<Node>)> on_disconnect_;
    std::function<void(DataDirectionMode, size_t)> on_data_;  // To gather receive data stats at higher level

    std::atomic_bool is_connected_{true};
    std::string read_buffer_{};

    std::atomic<size_t> bytes_received_{0};
    std::atomic<size_t> bytes_sent_{0};
};

}  // namespace zen::network
