/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "node.hpp"

#include <list>

#include <absl/strings/str_cat.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <boost/algorithm/clamp.hpp>
#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/misc.hpp>

#include <infra/common/log.hpp>
#include <infra/common/random.hpp>

namespace zenpp::net {

using namespace boost;
using asio::ip::tcp;

std::atomic_int Node::next_node_id_{1};  // Start from 1 for user-friendliness

Node::Node(AppSettings& app_settings, std::shared_ptr<Connection> connection_ptr, boost::asio::io_context& io_context,
           boost::asio::ssl::context* ssl_context, std::function<void(DataDirectionMode, size_t)> on_data,
           std::function<void(std::shared_ptr<Node>, std::shared_ptr<Message>)> on_message)
    : app_settings_(app_settings),
      connection_ptr_(std::move(connection_ptr)),
      io_strand_(io_context),
      ping_timer_(io_context, "Node_ping_timer", true),
      ssl_context_(ssl_context),
      on_data_(std::move(on_data)),
      on_message_(std::move(on_message)) {
    // TODO Set version's services according to settings
    local_version_.protocol_version_ = kDefaultProtocolVersion;
    local_version_.services_ = static_cast<uint64_t>(NodeServicesType::kNodeNetwork) bitor
                               static_cast<uint64_t>(NodeServicesType::kNodeGetUTXO);
    local_version_.timestamp_ = absl::ToUnixSeconds(absl::Now());
    local_version_.recipient_service_ = VersionNodeService(connection_ptr_->socket_ptr_->remote_endpoint());
    local_version_.sender_service_ = VersionNodeService(connection_ptr_->socket_ptr_->local_endpoint());

    // We use the same port declared in the settings or the one from the configured chain
    // if the former is not set
    const auto local_endpoint{IPEndpoint::from_string(app_settings_.network.local_endpoint)};
    ASSERT_POST(local_endpoint and "Invalid local endpoint");
    local_version_.sender_service_.endpoint_.port_ =
        local_endpoint.value().port_ == 0U ? app_settings_.chain_config->default_port_ : local_endpoint.value().port_;

    local_version_.nonce_ = app_settings_.network.nonce;
    local_version_.user_agent_ = get_buildinfo_string();
    local_version_.last_block_height_ = 0;  // TODO Set this value to the current blockchain height
    local_version_.relay_ = true;           // TODO Set this value from command line options
}

bool Node::start() noexcept {
    if (not Stoppable::start()) return false;

    local_endpoint_ = IPEndpoint(connection_ptr_->socket_ptr_->local_endpoint());
    remote_endpoint_ = IPEndpoint(connection_ptr_->socket_ptr_->remote_endpoint());
    const auto now{std::chrono::steady_clock::now()};
    last_message_received_time_.store(now);  // We don't want to disconnect immediately
    last_message_sent_time_.store(now);      // We don't want to disconnect immediately
    connected_time_.store(now);

    if (ssl_context_ not_eq nullptr) {
        ssl_stream_ = std::make_unique<asio::ssl::stream<tcp::socket&>>(*connection_ptr_->socket_ptr_, *ssl_context_);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_ssl_handshake(); });
    } else {
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
        std::ignore = push_message(local_version_);
    }
    return true;
}

bool Node::stop() noexcept {
    const auto ret{Stoppable::stop()};
    if (ret) /* not already stopped */ {
        ping_timer_.stop();
        if (ssl_stream_ not_eq nullptr) {
            boost::system::error_code error_code;
            std::ignore = ssl_stream_->shutdown(error_code);
        }
        asio::post(io_strand_, [self{shared_from_this()}]() { self->begin_stop(); });
    }
    return ret;
}

void Node::begin_stop() {
    boost::system::error_code error_code;
    if (ssl_stream_ not_eq nullptr) {
        std::ignore = ssl_stream_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, error_code);
        std::ignore = ssl_stream_->lowest_layer().close(error_code);
        // Don't reset the stream !!! There might be outstanding async operations
        // Let them gracefully complete
    } else {
        std::ignore = connection_ptr_->socket_ptr_->shutdown(asio::ip::tcp::socket::shutdown_both, error_code);
        std::ignore = connection_ptr_->socket_ptr_->close(error_code);
    }
    asio::post(io_strand_, [self{shared_from_this()}]() { self->on_stop_completed(); });
}

void Node::on_stop_completed() noexcept {
    if (log::test_verbosity(log::Level::kTrace)) {
        const std::list<std::string> log_params{"action", __func__, "status", "success"};
        print_log(log::Level::kTrace, log_params);
    }
    set_stopped();
}

void Node::on_ping_timer_expired(std::chrono::milliseconds& interval) noexcept {
    using namespace std::chrono_literals;
    if (ping_nonce_.load() not_eq 0U) return;  // Wait for response to return
    last_ping_sent_time_.store(std::chrono::steady_clock::time_point::min());
    ping_nonce_.store(randomize<uint64_t>(/*min=*/1U));
    MsgPingPayload ping_payload{};
    ping_payload.nonce_ = ping_nonce_.load();
    if (const auto result{push_message(ping_payload)}; result.has_error()) {
        const std::list<std::string> log_params{"action",  __func__, "status",
                                                "failure", "reason", result.error().message()};
        print_log(log::Level::kError, log_params, "Disconnecting ...");
        asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
        interval = 0ms;
        return;
    }
    const auto random_milliseconds{randomize<uint32_t>(app_settings_.network.ping_interval_seconds * 1'000U, 0.3)};
    interval = std::chrono::milliseconds(random_milliseconds);
}

void Node::process_ping_latency(const uint64_t latency_ms) {
    std::list<std::string> log_params{"action", __func__, "latency", absl::StrCat(latency_ms, "ms")};

    if (latency_ms > app_settings_.network.ping_timeout_milliseconds) {
        log_params.insert(log_params.end(),
                          {"max", absl::StrCat(app_settings_.network.ping_timeout_milliseconds, "ms")});
        print_log(log::Level::kWarning, log_params, "Timeout! Disconnecting ...");
        asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
        return;
    }

    const auto tmp_min_latency{min_ping_latency_.load()};
    if (tmp_min_latency == 0U) [[unlikely]] {
        min_ping_latency_.store(latency_ms);
    } else {
        min_ping_latency_.store(std::min(tmp_min_latency, latency_ms));
    }

    const auto tmp_ema_latency{ema_ping_latency_.load()};
    if (tmp_ema_latency == 0U) [[unlikely]] {
        ema_ping_latency_.store(latency_ms);
    } else {
        // Compute the EMA
        const auto alpha{0.65F};  // Newer values are more important
        const auto ema{alpha * static_cast<float>(latency_ms) + (1.0F - alpha) * static_cast<float>(tmp_ema_latency)};
        ema_ping_latency_.store(static_cast<uint64_t>(ema));
    }

    if (log::test_verbosity(log::Level::kTrace)) {
        log_params.insert(log_params.end(), {"min", absl::StrCat(min_ping_latency_.load(), "ms"), "ema",
                                             absl::StrCat(ema_ping_latency_.load(), "ms")});
        print_log(log::Level::kTrace, log_params);
    }

    ping_nonce_.store(0);
    last_ping_sent_time_.store(std::chrono::steady_clock::time_point::min());
}

void Node::start_ssl_handshake() {
    if (not connection_ptr_->socket_ptr_->is_open()) return;
    const asio::ssl::stream_base::handshake_type handshake_type{connection_ptr_->type_ == ConnectionType::kInbound
                                                                    ? asio::ssl::stream_base::server
                                                                    : asio::ssl::stream_base::client};
    ssl_stream_->set_verify_mode(asio::ssl::verify_none);
    ssl_stream_->async_handshake(handshake_type,
                                 [self{shared_from_this()}](const boost::system::error_code& error_code) {
                                     self->handle_ssl_handshake(error_code);
                                 });
}

void Node::handle_ssl_handshake(const boost::system::error_code& error_code) {
    if (error_code) {
        if (log::test_verbosity(log::Level::kWarning)) {
            const std::list<std::string> log_params{"action",  __func__, "status",
                                                    "failure", "reason", error_code.message()};
            print_log(log::Level::kWarning, log_params, "Disconnecting ...");
        }
        asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
        return;
    }
    if (log::test_verbosity(log::Level::kTrace)) {
        const std::list<std::string> log_params{"action", __func__, "status", "success"};
        print_log(log::Level::kTrace, log_params);
    }
    start_read();
    std::ignore = push_message(local_version_);
}

void Node::start_read() {
    if (not is_running()) return;
    auto read_handler{
        [self{shared_from_this()}](const boost::system::error_code& error_code, const size_t bytes_transferred) {
            self->handle_read(error_code, bytes_transferred);
        }};
    if (ssl_stream_ not_eq nullptr) {
        ssl_stream_->async_read_some(receive_buffer_.prepare(kMaxBytesPerIO), read_handler);
    } else {
        connection_ptr_->socket_ptr_->async_read_some(receive_buffer_.prepare(kMaxBytesPerIO), read_handler);
    }
}

void Node::handle_read(const boost::system::error_code& error_code, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (not is_running()) return;
    if (error_code) {
        const std::list<std::string> log_params{"action",  __func__, "status",
                                                "failure", "reason", error_code.message()};
        print_log(log::Level::kError, log_params, "Disconnecting ...");
        asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
        return;
    }

    if (bytes_transferred not_eq 0) {
        receive_buffer_.commit(bytes_transferred);
        bytes_received_ += bytes_transferred;
        on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        if (parse_result.has_error()) {
            const std::list<std::string> log_params{"action", __func__, "status", parse_result.error().message()};
            print_log(log::Level::kError, log_params, " Disconnecting ...");
            asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
            return;
        }
    }

    // Continue reading from socket
    asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
}

void Node::start_write() {
    if (not is_running()) return;
    if (bool expected{false}; not is_writing_.compare_exchange_strong(expected, true)) {
        return;  // Already writing - the queue will cause this to re-enter automatically
    }

    if (outbound_message_ not_eq nullptr and outbound_message_->data().eof()) {
        // A message has been fully sent - Exclude kPing and kPong
        const bool is_ping_pong{outbound_message_->header().get_type() == MessageType::kPing or
                                outbound_message_->header().get_type() == MessageType::kPong};
        if (not is_ping_pong) {
            last_message_sent_time_.store(std::chrono::steady_clock::now());
            outbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
        }
        outbound_message_.reset();
    }

    while (outbound_message_ == nullptr) {
        // Try to get a new message from the queue
        const std::scoped_lock lock{outbound_messages_mutex_};
        if (outbound_messages_.empty()) {
            is_writing_.store(false);
            return;  // Eventually next message submission to the queue will trigger a new write cycle
        }
        outbound_message_ = std::move(outbound_messages_.front());
        outbound_message_->data().seekg(0);
        outbound_messages_.erase(outbound_messages_.begin());
    }

    // If message has been just loaded into the barrel then we must check its validity
    // against protocol handshake rules
    if (outbound_message_->data().tellg() == 0) {
        const auto msg_type{outbound_message_->header().get_type()};
        if (log::test_verbosity(log::Level::kTrace)) {
            const std::list<std::string> log_params{
                "action",  __func__,
                "message", std::string(magic_enum::enum_name(outbound_message_->header().get_type())),
                "size",    to_human_bytes(outbound_message_->data().size())};
            print_log(log::Level::kTrace, log_params);
        }

        const auto result{validate_message_for_protocol_handshake(DataDirectionMode::kOutbound, msg_type)};
        if (result.has_error()) [[unlikely]] {
            if (log::test_verbosity(log::Level::kError)) {
                // TODO : Should we drop the connection here?
                // Actually outgoing messages' correct sequence is local responsibility
                // maybe we should either assert or push back the message into the queue
                const std::list<std::string> log_params{
                    "action", __func__,  "message", std::string(magic_enum::enum_name(msg_type)),
                    "status", "failure", "reason",  result.error().message()};
                print_log(log::Level::kError, log_params, "Disconnecting peer but is local fault ...");
            }
            outbound_message_.reset();
            is_writing_.exchange(false);
            asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
            return;
        }

        // Post actions to take on begin of outgoing message
        const auto now{std::chrono::steady_clock::now()};
        outbound_message_metrics_[msg_type].count_++;
        outbound_message_metrics_[msg_type].bytes_ += outbound_message_->data().size();
        switch (msg_type) {
            using enum MessageType;
            case kPing:
                last_ping_sent_time_.store(std::chrono::steady_clock::now());
                [[fallthrough]];
            case kPong:
                break;
            default:
                outbound_message_start_time_.store(now);
                break;
        }
    }

    // Push remaining data from the current message to the socket
    const auto bytes_to_write{std::min(kMaxBytesPerIO, outbound_message_->data().avail())};
    const auto data{outbound_message_->data().read(bytes_to_write)};
    ASSERT_POST(data and "Must have data to write");
    send_buffer_.sputn(reinterpret_cast<const char*>(data.value().data()),
                       static_cast<std::streamsize>(data.value().size()));

    auto write_handler{
        [self{shared_from_this()}](const boost::system::error_code& error_code, const size_t bytes_transferred) {
            self->handle_write(error_code, bytes_transferred);
        }};
    if (ssl_stream_ not_eq nullptr) {
        ssl_stream_->async_write_some(send_buffer_.data(), write_handler);
    } else {
        connection_ptr_->socket_ptr_->async_write_some(send_buffer_.data(), write_handler);
    }

    // We let handle_write to deal with re-entering the write cycle
}

void Node::handle_write(const boost::system::error_code& error_code, size_t bytes_transferred) {
    if (not is_running()) {
        is_writing_.store(false);
        return;
    }

    if (error_code) {
        if (log::test_verbosity(log::Level::kError)) {
            const std::list<std::string> log_params{"action",  __func__, "status",
                                                    "failure", "reason", error_code.message()};
            print_log(log::Level::kError, log_params, "Disconnecting ...");
        }
        is_writing_.exchange(false);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
        return;
    }

    if (bytes_transferred > 0U) {
        send_buffer_.consume(bytes_transferred);
        bytes_sent_ += bytes_transferred;
        on_data_(DataDirectionMode::kOutbound, bytes_transferred);
    }

    // If we have sent the whole message then we can start sending the next chunk
    if (send_buffer_.size() not_eq 0U) {
        auto write_handler{
            [self{shared_from_this()}](const boost::system::error_code& _error_code, const size_t _bytes_transferred) {
                self->handle_write(_error_code, _bytes_transferred);
            }};
        if (ssl_stream_ not_eq nullptr) {
            ssl_stream_->async_write_some(send_buffer_.data(), write_handler);
        } else {
            connection_ptr_->socket_ptr_->async_write_some(send_buffer_.data(), write_handler);
        }
    } else {
        is_writing_.store(false);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
    }
}

outcome::result<void> Node::push_message(MessagePayload& payload) {
    using namespace ser;
    using enum Error;

    auto new_message{std::make_unique<Message>(version_, app_settings_.chain_config->magic_)};
    auto result{new_message->push(payload)};
    if (result.has_error()) {
        if (log::test_verbosity(log::Level::kError)) {
            const std::list<std::string> log_params{
                "action", __func__,  "message", std::string(magic_enum::enum_name(payload.type())),
                "status", "failure", "reason",  result.error().message()};
            print_log(log::Level::kError, log_params);
        }
        return result.error();
    }
    std::unique_lock lock(outbound_messages_mutex_);
    outbound_messages_.emplace_back(new_message.release());
    lock.unlock();

    boost::asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
    return outcome::success();
}

outcome::result<void> Node::push_message(const MessageType message_type) {
    MsgNullPayload null_payload{message_type};
    return push_message(null_payload);
}

void Node::begin_inbound_message() {
    inbound_message_ = std::make_unique<Message>(version_, app_settings_.chain_config.value().magic_);
}

void Node::end_inbound_message() {
    inbound_message_.reset();
    inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
}

outcome::result<void> Node::parse_messages(const size_t bytes_transferred) {
    size_t messages_parsed{0};
    outcome::result<void> result{outcome::success()};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};

    while (!data.empty()) {
        if (inbound_message_ == nullptr) begin_inbound_message();
        result = inbound_message_->write(data);  // Note! data is consumed here
        if (result.has_error()) {
            if (result.error() == Error::kMessageHeaderIncomplete or result.error() == Error::kMessageBodyIncomplete) {
                // These two guys depict a normal condition and we must continue reading
                result = outcome::success();
            } else {
                break;
            }
        }

        // If we have the message type then we can validate it against protocol handshake rules
        const auto msg_type{inbound_message_->get_type()};
        if (msg_type.has_value()) {
            if (const auto handshake_result{
                    validate_message_for_protocol_handshake(DataDirectionMode::kInbound, msg_type.value())};
                handshake_result.has_error()) {
                result = handshake_result.error();
                break;
            }
        }

        if (not inbound_message_->is_complete()) continue;  // We need more data

        // We've got the header begin timing
        ASSERT(msg_type.has_value() and "Must have a valid message type");
        switch (msg_type.value()) {
            using enum MessageType;
            case kPing:
            case kPong:
                break;
            default:
                inbound_message_start_time_.store(std::chrono::steady_clock::now());
                break;
        }

        if (++messages_parsed > kMaxMessagesPerRead) {
            result = net::Error::kMessageFloodingDetected;
            break;
        }

        if (result = process_inbound_message(); result.has_error()) break;
        end_inbound_message();
    }

    receive_buffer_.consume(bytes_transferred);
    return result;
}

outcome::result<void> Node::process_inbound_message() {
    outcome::result<void> result{outcome::success()};
    std::string err_extended_reason{};
    bool notify_node_hub{false};

    ASSERT_PRE((inbound_message_ not_eq nullptr and inbound_message_->is_complete()) and "Must have a valid message");
    const auto msg_type{inbound_message_->header().get_type()};
    inbound_message_metrics_[msg_type].count_++;
    inbound_message_metrics_[msg_type].bytes_ += inbound_message_->data().size();

    switch (msg_type) {
        using enum MessageType;
        case kVersion:
            if (result = remote_version_.deserialize(inbound_message_->data()); result.has_error()) break;
            if (boost::algorithm::clamp(remote_version_.protocol_version_, kMinSupportedProtocolVersion,
                                        kMaxSupportedProtocolVersion) not_eq remote_version_.protocol_version_) {
                result = Error::kInvalidProtocolVersion;
                err_extended_reason =
                    absl::StrCat("Expected in range [", kMinSupportedProtocolVersion, ", ",
                                 kMaxSupportedProtocolVersion, "] got", remote_version_.protocol_version_, ".");
                break;
            }
            {
                version_.store(std::min(local_version_.protocol_version_, remote_version_.protocol_version_));
                const std::list<std::string> log_params{
                    "agent",    remote_version_.user_agent_,
                    "version",  std::to_string(remote_version_.protocol_version_),
                    "services", std::to_string(remote_version_.services_),
                    "relay",    (remote_version_.relay_ ? "true" : "false"),
                    "block",    std::to_string(remote_version_.last_block_height_),
                    "him",      remote_version_.sender_service_.endpoint_.to_string(),
                    "me",       remote_version_.recipient_service_.endpoint_.to_string()};
                if (remote_version_.nonce_ not_eq local_version_.nonce_) {
                    print_log(log::Level::kInfo, log_params);
                    result = push_message(MessageType::kVerAck);
                } else {
                    result = Error::kConnectedToSelf;
                    err_extended_reason = "Connected to self ? (same nonce)";
                }
            }
            break;
        case kVerAck:
            // This actually requires no action. Handshake flags already set
            // and we don't need to forward the message elsewhere
            break;
        case kPing: {
            MsgPingPayload ping_payload{};
            if (result = ping_payload.deserialize(inbound_message_->data()); not result.has_error()) {
                MsgPongPayload pong_payload(ping_payload);
                result = push_message(pong_payload);
            }
        } break;
        case kGetAddr:
            if (connection_ptr_->type_ == ConnectionType::kInbound and inbound_message_metrics_[kGetAddr].count_ > 1U) {
                // Ignore the message to avoid fingerprinting
                err_extended_reason = "Ignoring duplicate 'getaddr' message on inbound connection.";
                break;
            } else if (connection_ptr_->type_ == ConnectionType::kSeedOutbound) {
                // Disconnect from seed nodes as soon as we get some addresses from them
                asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(); });
            }
            notify_node_hub = true;
            break;
        case kPong: {
            const auto expected_nonce{ping_nonce_.load()};
            if (expected_nonce == 0U) {
                result = Error::kUnsolicitedPong;
                err_extended_reason = "Received an unrequested `pong` message.";
                break;
            }
            MsgPongPayload pong_payload{};
            if (result = pong_payload.deserialize(inbound_message_->data()); result.has_error()) break;
            if (pong_payload.nonce_ not_eq expected_nonce) {
                result = Error::kInvalidPingPongNonce;
                err_extended_reason = absl::StrCat("Expected ", expected_nonce, " got ", pong_payload.nonce_, ".");
                break;
            }
            // Calculate ping response time in milliseconds
            const auto time_now{std::chrono::steady_clock::now()};
            const auto ping_response_duration{time_now - last_ping_sent_time_.load()};
            const auto ping_response_time{
                std::chrono::duration_cast<std::chrono::milliseconds>(ping_response_duration)};

            // If the response time in milliseconds is greater than settings' threshold
            // this won't reset the timers and the nonce and let idle checks do their tasks
            process_ping_latency(static_cast<uint64_t>(ping_response_time.count()));

        } break;
        default:
            // Notify node-hub of the inbound message which will eventually take ownership of it
            // As a result we can safely reset it which is done after this function returns
            notify_node_hub = true;
            break;
    }

    if (result.has_error() or log::test_verbosity(log::Level::kTrace)) {
        const std::list<std::string> log_params{
            "action",  __func__,
            "message", std::string{magic_enum::enum_name(msg_type)},
            "size",    to_human_bytes(inbound_message_->size()),
            "status",  std::string(result.has_error() ? result.error().message() : "success")};
        print_log(result.has_error() ? log::Level::kWarning : log::Level::kTrace, log_params, err_extended_reason);
    }
    if (not result.has_error()) {
        if (msg_type not_eq MessageType::kPing and msg_type not_eq MessageType::kPong) {
            last_message_received_time_.store(std::chrono::steady_clock::now());
        }
        if (notify_node_hub) on_message_(shared_from_this(), std::move(inbound_message_));
    }
    return result;
}

outcome::result<void> Node::validate_message_for_protocol_handshake(const DataDirectionMode direction,
                                                                    const MessageType message_type) {
    using enum net::Error;

    // During protocol handshake we only allow version and verack messages
    // After protocol handshake we only allow other messages
    switch (message_type) {
        using enum MessageType;
        case kVersion:
        case kVerAck:
            if (protocol_handshake_status_ == ProtocolHandShakeStatus::kCompleted) return kDuplicateProtocolHandShake;
            break;  // Continue with validation
        default:
            if (protocol_handshake_status_ not_eq ProtocolHandShakeStatus::kCompleted) return kInvalidProtocolHandShake;
            return outcome::success();
    }

    uint32_t new_status_flag{0U};
    switch (direction) {
        using enum DataDirectionMode;
        using enum ProtocolHandShakeStatus;
        case kInbound:
            new_status_flag = static_cast<uint32_t>(message_type == MessageType::kVersion ? kRemoteVersionReceived
                                                                                          : kLocalVersionAckReceived);
            break;
        case kOutbound:
            new_status_flag = static_cast<uint32_t>(message_type == MessageType::kVersion ? kLocalVersionSent
                                                                                          : kRemoteVersionAckSent);
            break;
    }

    const auto status{static_cast<uint32_t>(protocol_handshake_status_.load())};
    if ((status & new_status_flag) == new_status_flag) return kDuplicateProtocolHandShake;
    protocol_handshake_status_.store(static_cast<ProtocolHandShakeStatus>(status | new_status_flag));
    if (protocol_handshake_status_ == ProtocolHandShakeStatus::kCompleted) {
        // Happens only once per session
        on_handshake_completed();
    }

    return outcome::success();
}

void Node::on_handshake_completed() {
    if (not is_running()) return;

    // If this is a seeder node then we should async_send a `getaddr` message
    if (connection_ptr_->type_ == ConnectionType::kSeedOutbound) {
        std::ignore = push_message(MessageType::kGetAddr);
    }

    // Lets' async_send out a ping immediately and start the timer
    // for subsequent pings
    const auto random_milliseconds{randomize<uint32_t>(app_settings_.network.ping_interval_seconds * 1'000U, 0.3)};
    auto ping_interval = std::chrono::milliseconds(random_milliseconds);
    on_ping_timer_expired(ping_interval);
    ping_timer_.autoreset(true);
    ping_timer_.start(ping_interval, [this](std::chrono::milliseconds& interval) { on_ping_timer_expired(interval); });
}

NodeIdleResult Node::is_idle() const noexcept {
    using enum NodeIdleResult;
    using namespace std::chrono;

    if (!is_connected()) return kNotIdle;  // Not connected - not idle

    const auto now{steady_clock::now()};

    // Check whether we're waiting for a ping response
    if (ping_nonce_.load() not_eq 0) {
        const auto ping_duration{duration_cast<milliseconds>(now - last_ping_sent_time_.load()).count()};
        if (ping_duration > app_settings_.network.ping_timeout_milliseconds) {
            const std::list<std::string> log_params{
                "action",  __func__,
                "status",  "ping timeout",
                "latency", absl::StrCat(ping_duration, "ms"),
                "max",     absl::StrCat(app_settings_.network.ping_timeout_milliseconds, "ms")};
            print_log(log::Level::kDebug, log_params, "Disconnecting ...");
            return kPingTimeout;
        }
    }

    // Check we've at least completed protocol handshake in a reasonable time
    if (protocol_handshake_status_.load() not_eq ProtocolHandShakeStatus::kCompleted) {
        const auto handshake_duration{duration_cast<seconds>(now - connected_time_.load()).count()};
        if (handshake_duration > app_settings_.network.protocol_handshake_timeout_seconds) {
            const std::list<std::string> log_params{
                "action",   __func__,
                "status",   "handshake timeout",
                "duration", absl::StrCat(handshake_duration, "s"),
                "max",      absl::StrCat(app_settings_.network.protocol_handshake_timeout_seconds, "s")};
            print_log(log::Level::kDebug, log_params, "Disconnecting ...");
            return kProtocolHandshakeTimeout;
        }
    }

    // Check whether there's an inbound message in progress
    if (const auto value{inbound_message_start_time_.load()}; value not_eq steady_clock::time_point::min()) {
        const auto inbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (inbound_message_duration > app_settings_.network.inbound_timeout_seconds) {
            const std::list<std::string> log_params{
                "action",   __func__,
                "status",   "inbound timeout",
                "duration", absl::StrCat(inbound_message_duration, "s"),
                "max",      absl::StrCat(app_settings_.network.inbound_timeout_seconds, "s")};
            print_log(log::Level::kDebug, log_params, "Disconnecting ...");
            return kInboundTimeout;
        }
    }

    // Check whether there's an outbound message in progress
    if (const auto value{outbound_message_start_time_.load()}; value not_eq steady_clock::time_point::min()) {
        const auto outbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (outbound_message_duration > app_settings_.network.outbound_timeout_seconds) {
            const std::list<std::string> log_params{
                "action",   __func__,
                "status",   "outbound timeout",
                "duration", absl::StrCat(outbound_message_duration, "s"),
                "max",      absl::StrCat(app_settings_.network.outbound_timeout_seconds, "s")};
            print_log(log::Level::kDebug, log_params, "Disconnecting ...");
            return kOutboundTimeout;
        }
    }

    // Check whether there's been any activity
    const auto most_recent_activity_time{std::max(last_message_received_time_.load(), last_message_sent_time_.load())};
    const auto idle_seconds{duration_cast<seconds>(now - most_recent_activity_time).count()};
    if (static_cast<uint32_t>(idle_seconds) >= app_settings_.network.idle_timeout_seconds) {
        const std::list<std::string> log_params{
            "action",   __func__,
            "status",   "inactivity timeout",
            "duration", absl::StrCat(idle_seconds, "s"),
            "max",      absl::StrCat(app_settings_.network.idle_timeout_seconds, "s")};
        print_log(log::Level::kDebug, log_params, "Disconnecting ...");
        return kGlobalTimeout;
    }

    return kNotIdle;
}

std::string Node::to_string() const noexcept { return remote_endpoint_.to_string(); }

void Node::print_log(const log::Level severity, const std::list<std::string>& params,
                     const std::string& extra_data) const noexcept {
    if (not log::test_verbosity(severity)) return;
    std::vector<std::string> log_params{"id", std::to_string(node_id_), "remote", this->to_string()};
    log_params.insert(log_params.end(), params.begin(), params.end());
    log::BufferBase(severity, "Node", log_params) << extra_data;
}

}  // namespace zenpp::net
