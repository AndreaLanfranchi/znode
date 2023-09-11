/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <gsl/pointers>
#include <magic_enum.hpp>
#include <openssl/ssl.h>

#include <core/abi/netmessage.hpp>
#include <core/common/base.hpp>
#include <core/types/network.hpp>

#include <app/common/settings.hpp>
#include <app/concurrency/asio_timer.hpp>
#include <app/network/protocol.hpp>

namespace zenpp::network {

// enum class NodeConnectionMode {
//     kInbound,         // Node is accepting connections
//     kOutbound,        // Node is connecting to other nodes
//     kManualOutbound,  // Forced connection to a specific node
//     kSeedOutbound     // Connection to a seed node
// };

enum class NodeIdleResult {
    kNotIdle,                   // Node is not idle
    kProtocolHandshakeTimeout,  // Too much time for protocol handshake
    kInboundTimeout,            // Too much time since the beginning of an inbound message
    kOutboundTimeout,           // Too much time since the beginning of an outbound message
    kPingTimeout,               // Too much time since the last ping message
    kGlobalTimeout              // Too much time since the last completed activity
};

enum class DataDirectionMode {
    kInbound,
    kOutbound
};

//! \brief Maximum number of messages to parse in a single read operation
static constexpr size_t kMaxMessagesPerRead = 32;

//! \brief Maximum number of bytes to read/write in a single operation
static constexpr size_t kMaxBytesPerIO = 64_KiB;

class Node : public Stoppable, public std::enable_shared_from_this<Node> {
  public:
    Node(AppSettings& app_settings, IPConnection connection, boost::asio::io_context& io_context,
         boost::asio::ip::tcp::socket socket, boost::asio::ssl::context* ssl_context,
         std::function<void(DataDirectionMode, size_t)> on_data /* handles data size accounting on node-hub */,
         std::function<void(std::shared_ptr<Node>, std::shared_ptr<abi::NetMessage>)>
             on_message /* handles connections on node-hub */);

    Node(Node& other) = delete;
    Node(Node&& other) = delete;
    Node& operator=(const Node& other) = delete;
    ~Node() override = default;

    //! \brief Start the asynchronous read/write operations
    bool start() noexcept override;

    //! \brief Stops the asynchronous read/write operations and disconnects
    bool stop(bool wait) noexcept override;

    //! \brief Used as custom deleter for shared_ptr<Node> to ensure proper closure of the socket
    //! \attention Do not call directly, use in std::shared_ptr<Node> instead
    static void clean_up(gsl::owner<Node*> ptr) noexcept;

    enum class ProtocolHandShakeStatus : uint32_t {
        kNotInitiated = 0,                  // 0
        kLocalVersionSent = 1 << 0,         // 1
        kLocalVersionAckReceived = 1 << 1,  // 2
        kRemoteVersionReceived = 1 << 2,    // 4
        kRemoteVersionAckSent = 1 << 3,     // 8
        kCompleted = kLocalVersionSent | kLocalVersionAckReceived | kRemoteVersionReceived | kRemoteVersionAckSent
    };

    //! \return The unique identifier of the node
    [[nodiscard]] int id() const noexcept { return node_id_; }

    //! \brief Whether this node instance is inbound or outbound
    [[nodiscard]] IPConnection connection() const noexcept { return connection_; }

    //! \brief Returns a reference to the underlying socket
    [[nodiscard]] boost::asio::ip::tcp::socket& socket() noexcept { return socket_; }

    //! \brief Returns whether the connection is secure
    [[nodiscard]] bool is_secure() const noexcept { return ssl_context_ != nullptr; }

    //! \brief Returns the remote endpoint
    [[nodiscard]] IPEndpoint remote_endpoint() const noexcept { return remote_endpoint_; }

    //! \brief Returns the local endpoint
    [[nodiscard]] IPEndpoint local_endpoint() const noexcept { return local_endpoint_; }

    //! \brief The actual status of the protocol handshake
    [[nodiscard]] ProtocolHandShakeStatus get_protocol_handshake_status() const noexcept {
        return protocol_handshake_status_.load();
    }

    //! \brief Returns whether the socket is connected and the protocol handshake is completed
    [[nodiscard]] bool is_connected() const noexcept {
        return is_running() && socket_.is_open() &&
               get_protocol_handshake_status() == ProtocolHandShakeStatus::kCompleted;
    }

    //! \brief Returns information about the received Version message
    [[nodiscard]] const abi::MsgVersionPayload& version_info() const noexcept { return remote_version_; }

    //! \brief Returns whether the remote node supports the specified service
    [[nodiscard]] bool has_service(NodeServicesType service) const noexcept {
        return (is_connected() && (((local_version_.services_ & static_cast<uint64_t>(service)) != 0)));
    }

    //! \brief Returns the average ping latency in milliseconds
    [[nodiscard]] uint64_t ping_latency() const noexcept { return ema_ping_latency_.load(); }

    //! \brief Returns whether the node (i.e. the remote) has been inactive/unresponsive beyond the amounts
    //! of time specified in network settings
    [[nodiscard]] NodeIdleResult is_idle() const noexcept;

    //! \brief Returns whether the node as advertised himself as a relayer (Version message)
    [[nodiscard]] bool is_relayer() const noexcept { return (is_connected() && local_version_.relay_); }

    //! \brief Returns the total number of bytes read from the socket
    [[nodiscard]] size_t bytes_received() const noexcept { return bytes_received_.load(); }

    //! \brief Returns the total number of bytes written to the socket
    [[nodiscard]] size_t bytes_sent() const noexcept { return bytes_sent_.load(); }

    //! \return The string representation of the remote endpoint
    [[nodiscard]] std::string to_string() const noexcept;

    //! \return The next available node id
    [[nodiscard]] static int next_node_id() noexcept { return next_node_id_.fetch_add(1); }

    //! \brief Creates a new network message to be queued for delivery to the remote node
    serialization::Error push_message(abi::NetMessageType message_type, abi::NetMessagePayload& payload);

    //! \brief Creates a new network message to be queued for delivery to the remote node
    //! \remarks This a handy overload used to send messages with a null payload
    serialization::Error push_message(abi::NetMessageType message_type);

  private:
    void start_ssl_handshake();
    void handle_ssl_handshake(const boost::system::error_code& error_code);

    void begin_inbound_message();
    void end_inbound_message();

    //! \brief Sends a ping to the remote peer on cadence
    //! \remark The interval is randomly chosen on setting's ping get_interval +/- 30%
    uint32_t on_ping_timer_expired(uint32_t interval_milliseconds) noexcept;

    //! \brief Computes the ping latency and updates the EMA
    //! \details Ping latency is the get_interval between the sending of a ping message and the reception of the
    //! corresponding pong message
    void process_ping_latency(uint64_t latency_ms);

    void start_read();
    void handle_read(const boost::system::error_code& error_code, size_t bytes_transferred);
    serialization::Error parse_messages(
        size_t bytes_transferred);  // Reads messages from the receiving buffer and consumes buffered data

    serialization::Error process_inbound_message();  // Local processing (when possible) of inbound message

    //! \brief Returns whether the message is acceptable in the current state of the protocol handshake
    //! \remarks Every message (inbound or outbound) MUST be validated by this before being further processed
    [[nodiscard]] serialization::Error validate_message_for_protocol_handshake(DataDirectionMode direction,
                                                                               abi::NetMessageType message_type);

    void on_handshake_completed();  // Called when the protocol handshake is completed

    void start_write();  // Begin writing to the socket asynchronously
    void handle_write(const boost::system::error_code& error_code, size_t bytes_transferred);  // Async write handler

    //! \brief Local facility to print log lines in unified format
    void print_log(log::Level severity, const std::list<std::string>& params,
                   const std::string& extra_data = "") const noexcept;

    AppSettings& app_settings_;                      // Reference to global application settings
    static std::atomic_int next_node_id_;            // Used to generate unique node ids
    const int node_id_{next_node_id()};              // Unique node id
    const IPConnection connection_;                  // Whether inbound or outbound
    boost::asio::io_context::strand input_strand_;   // Serialized execution of reads
    boost::asio::io_context::strand output_strand_;  // Serialized execution of writes
    AsioTimer ping_timer_;                           // To periodically send ping messages
    boost::asio::ip::tcp::socket socket_;            // The underlying socket (either plain or SSL)
    IPEndpoint remote_endpoint_;                     // Remote endpoint
    IPEndpoint local_endpoint_;                      // Local endpoint

    boost::asio::ssl::context* ssl_context_;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> ssl_stream_;  // SSL stream

    std::atomic<ProtocolHandShakeStatus> protocol_handshake_status_{
        ProtocolHandShakeStatus::kNotInitiated};                         // Status of protocol handshake
    std::atomic_int version_{kDefaultProtocolVersion};                   // Protocol version negotiated during handshake
    std::atomic<std::chrono::steady_clock::time_point> connected_time_;  // Time of connection
    std::atomic<std::chrono::steady_clock::time_point> last_message_received_time_;  // Last fully "in" message tstamp
    std::atomic<std::chrono::steady_clock::time_point> last_message_sent_time_;      // Last fully "out" message

    std::atomic<std::chrono::steady_clock::time_point> last_ping_sent_time_;  // Last outgoing ping tstamp
    std::atomic_uint64_t ping_nonce_{0};                                      // Last ping nonce sent
    std::atomic_uint32_t ping_samples_count_{0};                              // Number of ping samples
    std::atomic_uint64_t min_ping_latency_{0};                                // Minimum ping latency
    std::atomic_uint64_t ema_ping_latency_{0};  // Exponential moving average of ping latency

    std::function<void(DataDirectionMode, size_t)> on_data_;  // To account data sizes stats at node hub
    std::function<void(std::shared_ptr<Node>, std::shared_ptr<abi::NetMessage>)>
        on_message_;  // Called on inbound message

    boost::asio::streambuf receive_buffer_;  // Socket receive buffer
    boost::asio::streambuf send_buffer_;     // Socket send buffer
    std::atomic<size_t> bytes_received_{0};  // Total bytes received from the socket during the session
    std::atomic<size_t> bytes_sent_{0};      // Total bytes sent to the socket during the session

    std::atomic<std::chrono::steady_clock::time_point> inbound_message_start_time_{
        std::chrono::steady_clock::time_point::min()};           // Start time of inbound msg
    std::unique_ptr<abi::NetMessage> inbound_message_{nullptr};  // The "next" message being received

    std::atomic_bool is_writing_{false};  // Whether a write operation is in progress
    std::atomic<std::chrono::steady_clock::time_point> outbound_message_start_time_{
        std::chrono::steady_clock::time_point::min()};              // Start time of outbound msg
    std::shared_ptr<abi::NetMessage> outbound_message_{nullptr};    // The "next" message being sent
    std::vector<decltype(outbound_message_)> outbound_messages_{};  // Queue of messages awaiting to be sent
    std::mutex outbound_messages_mutex_{};                          // Lock guard for messages to be sent

    abi::MsgVersionPayload local_version_{};   // Local protocol version
    abi::MsgVersionPayload remote_version_{};  // Remote protocol version

    struct MessageMetrics {
        std::atomic_uint32_t count_{0};
        std::atomic<size_t> bytes_{0};
    };

    std::map<abi::NetMessageType, MessageMetrics> inbound_message_metrics_{};   // Stats for each message type
    std::map<abi::NetMessageType, MessageMetrics> outbound_message_metrics_{};  // Stats for each message type
};
}  // namespace zenpp::network
