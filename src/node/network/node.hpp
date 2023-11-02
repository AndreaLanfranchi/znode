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

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <magic_enum.hpp>
#include <openssl/ssl.h>

#include <core/common/base.hpp>

#include <infra/concurrency/timer.hpp>
#include <infra/network/message.hpp>
#include <infra/network/ping_meter.hpp>
#include <infra/network/protocol.hpp>
#include <infra/network/traffic_meter.hpp>

#include <node/common/settings.hpp>
#include <node/network/connection.hpp>

namespace znode::net {

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

//! \brief A node holds a connection (and related session) to a remote peer
class Node : public con::Stoppable, public std::enable_shared_from_this<Node> {
  public:
    Node(AppSettings& app_settings, std::shared_ptr<Connection> connection_ptr, boost::asio::io_context& io_context,
         boost::asio::ssl::context* ssl_context, std::function<void(DataDirectionMode, size_t)> on_data,
         std::function<void(std::shared_ptr<Node>, std::shared_ptr<MessagePayload>)> on_message);

    Node(Node& other) = delete;
    Node(Node&& other) = delete;
    Node& operator=(const Node& other) = delete;
    ~Node() override = default;

    //! \brief Start the asynchronous read/write operations
    bool start() noexcept override;

    //! \brief Stops the asynchronous read/write operations and disconnects
    bool stop() noexcept override;

    //! \brief Flags describing the status of the protocol handshake
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
    [[nodiscard]] const Connection& connection() const noexcept { return *connection_ptr_; }

    //! \brief Returns whether the connection is secure
    [[nodiscard]] bool is_secure() const noexcept { return ssl_context_ != nullptr; }

    //! \brief Returns the remote endpoint
    [[nodiscard]] IPEndpoint remote_endpoint() const noexcept { return remote_endpoint_; }

    //! \brief Returns the local endpoint
    [[nodiscard]] IPEndpoint local_endpoint() const noexcept { return local_endpoint_; }

    //! \brief Returns whether the socket is connected and the protocol handshake is completed
    [[nodiscard]] bool fully_connected() const noexcept {
        return is_running() && connection_ptr_->socket_ptr_->is_open() &&
               protocol_handshake_status_.load() == ProtocolHandShakeStatus::kCompleted;
    }

    //! \brief Returns the total duration of current connection
    [[nodiscard]] std::chrono::steady_clock::duration connection_duration() const noexcept {
        return std::chrono::steady_clock::now() - connected_time_.load();
    }

    //! \brief Returns information about the received Version message
    [[nodiscard]] const MsgVersionPayload& version_info() const noexcept { return remote_version_; }

    //! \brief Returns whether the remote node supports the specified service
    [[nodiscard]] bool has_service(NodeServicesType service) const noexcept {
        return (fully_connected() && ((local_version_.services_ & static_cast<uint64_t>(service)) != 0));
    }

    //! \brief Returns the average ping latency in milliseconds
    [[nodiscard]] std::chrono::milliseconds ping_latency() const noexcept { return ping_meter_.get_ema(); }

    //! \brief Returns whether the node (i.e. the remote) has been inactive/unresponsive beyond the amounts
    //! of time specified in network settings
    [[nodiscard]] NodeIdleResult is_idle() const noexcept;

    //! \brief Returns whether the node as advertised himself as a relayer (Version message)
    [[nodiscard]] bool is_relayer() const noexcept { return (fully_connected() && local_version_.relay_); }

    //! \return The string representation of the remote endpoint
    [[nodiscard]] std::string to_string() const noexcept;

    //! \return The next available node id
    [[nodiscard]] static int next_node_id() noexcept { return next_node_id_.fetch_add(1); }

    //! \brief Creates a new network message to be queued for delivery to the remote node
    outcome::result<void> push_message(MessagePayload& payload, MessagePriority priority = MessagePriority::kNormal);

    //! \brief Creates a new network message to be queued for delivery to the remote node
    //! \remarks This a handy overload used to async_send messages with a null payload
    outcome::result<void> push_message(MessageType message_type, MessagePriority priority = MessagePriority::kNormal);

  private:
    void start_ssl_handshake();
    void handle_ssl_handshake(const boost::system::error_code& error_code);

    //! \brief Sends a ping to the remote peer on cadence
    //! \remark The interval is randomly chosen on setting's ping get_interval +/- 30%
    void on_ping_timer_expired(con::Timer::duration& interval) noexcept;

    void start_read();
    void handle_read(const boost::system::error_code& error_code, size_t bytes_transferred);
    outcome::result<void> parse_messages(
        size_t bytes_transferred);  // Reads messages from the receiving buffer and consumes buffered data

    outcome::result<void> process_inbound_message(
        std::shared_ptr<MessagePayload> payload_ptr);  // Local processing (when possible) of inbound message

    //! \brief Returns whether the message is acceptable in the current state of the protocol handshake
    //! \remarks Every message (inbound or outbound) MUST be validated by this before being further processed
    [[nodiscard]] outcome::result<void> validate_message_for_protocol_handshake(DataDirectionMode direction,
                                                                                MessageType message_type);

    //! \brief To be called as soon as the protocol handshake is completed
    //! \details This is the place where the node is considered fully connected and could start sending/receiving
    //! messages. Also ping timer is started here.
    void on_handshake_completed();

    void start_write();  // Begin writing to the socket asynchronously
    void handle_write(const boost::system::error_code& error_code, size_t bytes_transferred);  // Async write handler

    void on_stop_completed() noexcept;  // Called when the node is stopped

    //! \brief Local facility to print log lines in unified format
    void print_log(log::Level severity, const std::list<std::string>& params,
                   const std::string& extra_data = "") const noexcept;

    AppSettings& app_settings_;                   // Reference to global application settings
    static std::atomic_int next_node_id_;         // Used to generate unique node ids
    const int node_id_{next_node_id()};           // Unique node id
    std::shared_ptr<Connection> connection_ptr_;  // Connection specs
    boost::asio::io_context::strand io_strand_;   // Serialized execution of reads and writes
    con::Timer ping_timer_;                       // To periodically async_send ping messages
    IPEndpoint remote_endpoint_;                  // Remote endpoint
    IPEndpoint local_endpoint_;                   // Local endpoint
    net::TrafficMeter traffic_meter_{};           // Traffic meter
    net::PingMeter ping_meter_{};                 // Ping meter

    boost::asio::ssl::context* ssl_context_;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> ssl_stream_;  // SSL stream

    std::atomic<ProtocolHandShakeStatus> protocol_handshake_status_{
        ProtocolHandShakeStatus::kNotInitiated};                         // Status of protocol handshake
    std::atomic_int version_{kDefaultProtocolVersion};                   // Protocol version negotiated during handshake
    std::atomic<std::chrono::steady_clock::time_point> connected_time_;  // Time of connection
    std::atomic<std::chrono::steady_clock::time_point> last_message_received_time_;  // Last fully "in" message tstamp
    std::atomic<std::chrono::steady_clock::time_point> last_message_sent_time_;      // Last fully "out" message

    std::function<void(DataDirectionMode, size_t)> on_data_;  // To account data sizes stats at node hub
    std::function<void(std::shared_ptr<Node>, std::shared_ptr<MessagePayload>)>
        on_message_;  // Called on inbound message

    boost::asio::streambuf receive_buffer_;  // Socket async_receive buffer
    boost::asio::streambuf send_buffer_;     // Socket async_send buffer

    std::atomic<std::chrono::steady_clock::time_point> inbound_message_start_time_{
        std::chrono::steady_clock::time_point::min()};   // Start time of inbound msg
    std::unique_ptr<Message> inbound_message_{nullptr};  // The "next" message being received

    std::atomic_bool is_writing_{false};  // Whether a write operation is in progress
    std::atomic<std::chrono::steady_clock::time_point> outbound_message_start_time_{
        std::chrono::steady_clock::time_point::min()};  // Start time of outbound msg

    using message_queue_item = std::pair<std::shared_ptr<Message>, MessagePriority>;
    struct MessageQueueItemComparator {
        bool operator()(const message_queue_item& lhs, const message_queue_item& rhs) const {
            return static_cast<int>(lhs.second) < static_cast<int>(rhs.second);
        }
    };
    std::priority_queue<message_queue_item, std::vector<message_queue_item>, MessageQueueItemComparator>
        outbound_messages_queue_{};  // Queue of messages awaiting to be sent

    std::shared_ptr<Message> outbound_message_{nullptr};  // The "next" message being sent
    std::mutex outbound_messages_mutex_{};                // Lock guard for messages to be sent

    MsgVersionPayload local_version_{};   // Local protocol version
    MsgVersionPayload remote_version_{};  // Remote protocol version

    struct MessageMetrics {
        std::atomic_uint32_t count_{0};
        std::atomic<size_t> bytes_{0};
    };

    std::map<MessageType, MessageMetrics> inbound_message_metrics_{};   // Stats for each message type
    std::map<MessageType, MessageMetrics> outbound_message_metrics_{};  // Stats for each message type
};
}  // namespace znode::net
