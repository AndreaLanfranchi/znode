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

#include "node.hpp"

#include <list>

#include <absl/strings/str_cat.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <boost/algorithm/clamp.hpp>
#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/misc.hpp>
#include <core/common/random.hpp>

#include <infra/common/common.hpp>
#include <infra/common/log.hpp>
#include <infra/common/stopwatch.hpp>

namespace znode::net {

using namespace boost;
using asio::ip::tcp;

std::atomic_int Node::next_node_id_{1};  // Start from 1 for user-friendliness

Node::Node(AppSettings& app_settings, std::shared_ptr<Connection> connection_ptr, boost::asio::io_context& io_context,
           boost::asio::ssl::context* ssl_context,                                                  //
           std::function<void(DataDirectionMode, size_t)> on_data,                                  //
           std::function<void(std::shared_ptr<Node>, std::shared_ptr<MessagePayload>)> on_message,  //
           std::function<void(const Node&)> on_disconnected)
    : app_settings_(app_settings),
      connection_ptr_(std::move(connection_ptr)),
      io_strand_(io_context),
      ping_timer_(io_context, "Node_ping_timer", true),
      ssl_context_(ssl_context),
      on_data_(std::move(on_data)),
      on_message_(std::move(on_message)),
      on_disconnected_(std::move(on_disconnected)) {
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
        // If connection is inbound we wait for the remote to send its version message first
        // Otherwise we present ourselves first
        if (connection_ptr_->type_ not_eq ConnectionType::kInbound) {
            std::ignore = push_message(local_version_);
        }
    }
    return true;
}

bool Node::stop() noexcept {
    const auto ret{Stoppable::stop()};
    if (ret) /* not already stopped */ {
        ping_timer_.stop();
        boost::system::error_code error_code;
        if (ssl_stream_ not_eq nullptr) {
            std::ignore = ssl_stream_->lowest_layer().cancel(error_code);
            std::ignore = ssl_stream_->shutdown(error_code);
        } else {
            std::ignore = connection_ptr_->socket_ptr_->cancel(error_code);
        }
        asio::post(io_strand_, [self{shared_from_this()}]() { self->on_stop_completed(); });
    }
    return ret;
}

void Node::on_stop_completed() noexcept {
    boost::system::error_code error_code;
    if (ssl_stream_ not_eq nullptr) {
        std::ignore = ssl_stream_->lowest_layer().close(error_code);
    } else {
        std::ignore = connection_ptr_->socket_ptr_->close(error_code);
    }

    on_disconnected_(*this);

    if (log::test_verbosity(log::Level::kTrace)) {
        const std::list<std::string> log_params{"action", __func__, "status", "success"};
        print_log(log::Level::kTrace, log_params);
    }

    const auto [inbound_bytes, outbound_bytes]{traffic_meter_.get_cumulative_bytes()};
    std::ignore = log::Info(
        "Node", {"id", std::to_string(node_id_), "remote", to_string(), "status", "disconnected", "data i/o",
                 absl::StrCat(to_human_bytes(inbound_bytes, true), " ", to_human_bytes(outbound_bytes, true))});
    set_stopped();
}

void Node::on_ping_timer_expired(con::Timer::duration& interval) noexcept {
    using namespace std::chrono_literals;
    if (not is_running() or ping_meter_.pending_sample()) return;  // Wait for response to return
    const auto nonce{randomize<uint64_t>(/*min=*/1U)};
    MsgPingPongPayload ping_payload{MessageType::kPing, nonce};
    if (const auto result{push_message(ping_payload, MessagePriority::kHigh)}; result.has_error()) {
        const std::list<std::string> log_params{"action",  __func__, "status",
                                                "failure", "reason", result.error().message()};
        print_log(log::Level::kError, log_params, "Disconnecting ...");
        std::ignore = stop();
        interval = 0ms;
        return;
    }
    ping_meter_.set_nonce(nonce);
    const auto random_milliseconds{randomize<uint32_t>(app_settings_.network.ping_interval_seconds * 1'000U, 0.3)};
    interval = std::chrono::milliseconds(random_milliseconds);
}

void Node::start_ssl_handshake() {
    if (not connection_ptr_->socket_ptr_->is_open()) return;
    const asio::ssl::stream_base::handshake_type handshake_type{connection_ptr_->type_ == ConnectionType::kInbound
                                                                    ? asio::ssl::stream_base::server
                                                                    : asio::ssl::stream_base::client};
    ssl_stream_->set_verify_mode(asio::ssl::verify_none);  // TODO : Set verify mode according to settings
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

    // If connection is inbound we wait for the remote to send its version message first
    // Otherwise we present ourselves first
    if (connection_ptr_->type_ not_eq ConnectionType::kInbound) {
        std::ignore = push_message(local_version_);
    }
}

void Node::start_read() {
    if (not connection_ptr_->socket_ptr_->is_open()) return;
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
        traffic_meter_.update_inbound(bytes_transferred);
        on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        receive_buffer_.consume(bytes_transferred);
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
    if (not connection_ptr_->socket_ptr_->is_open()) return;
    if (bool expected{false}; not is_writing_.compare_exchange_strong(expected, true)) {
        return;  // Already writing - the queue will cause this to re-enter automatically
    }

    if (outbound_message_ not_eq nullptr and outbound_message_->data().eof()) {
        // Don't time ping pong messages
        if (outbound_message_->get_type() not_eq MessageType::kPing and
            outbound_message_->get_type() not_eq MessageType::kPong) {
            last_message_sent_time_.store(std::chrono::steady_clock::now());
        }
        outbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
        outbound_message_.reset();
    }

    while (outbound_message_ == nullptr) {
        // Try to get a new message from the queue
        const std::scoped_lock lock{outbound_messages_mutex_};
        if (outbound_messages_queue_.empty()) {
            is_writing_.exchange(false);
            return;  // Eventually next message submission to the queue will trigger a new write cycle
        }
        outbound_message_ = outbound_messages_queue_.top().first;
        outbound_messages_queue_.pop();
        outbound_message_->data().seekg(0);
    }

    // If message has been just loaded into the barrel then we must check its validity
    // against protocol handshake rules
    if (outbound_message_->data().tellg() == 0) {
        const auto msg_type{outbound_message_->header().get_type()};
        const auto command{command_from_message_type(msg_type)};
        if (log::test_verbosity(log::Level::kTrace)) {
            const std::list<std::string> log_params{
                "action", __func__, "command", command, "size", to_human_bytes(outbound_message_->data().size())};
            print_log(log::Level::kTrace, log_params);
        }

        const auto result{validate_message_for_protocol_handshake(DataDirectionMode::kOutbound, msg_type)};
        if (result.has_error()) [[unlikely]] {
            if (log::test_verbosity(log::Level::kError)) {
                // TODO : Should we drop the connection here?
                // Actually outgoing messages' correct sequence is local responsibility
                // maybe we should either assert or push back the message into the queue
                const std::list<std::string> log_params{"action", __func__,  "command", command,
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
                ping_meter_.start_sample();
                [[fallthrough]];
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
        is_writing_.exchange(false);
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
        traffic_meter_.update_outbound(bytes_transferred);
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
        is_writing_.exchange(false);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
    }
}

outcome::result<void> Node::push_message(MessagePayload& payload, MessagePriority priority) {
    using namespace ser;
    using enum Error;

    auto new_message{std::make_unique<Message>(version_, app_settings_.chain_config->magic_)};
    if (const auto result{new_message->push(payload)}; result.has_error()) {
        if (log::test_verbosity(log::Level::kError)) {
            const std::list<std::string> log_params{
                "action", __func__,  "message", std::string(magic_enum::enum_name(payload.type())),
                "status", "failure", "reason",  result.error().message()};
            print_log(log::Level::kError, log_params);
        }
        return result.error();
    }
    std::unique_lock lock(outbound_messages_mutex_);
    outbound_messages_queue_.emplace(std::move(new_message), priority);
    lock.unlock();

    boost::asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
    return outcome::success();
}

outcome::result<void> Node::push_message(const MessageType message_type, MessagePriority priority) {
    MsgNullPayload null_payload{message_type};
    return push_message(null_payload, priority);
}

outcome::result<void> Node::parse_messages(const size_t bytes_transferred) {
    size_t messages_parsed{0};
    outcome::result<void> result{outcome::success()};
    MessageType msg_type{MessageType::kMissingOrUnknown};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};

    while (!data.empty()) {
        if (inbound_message_ == nullptr) {
            inbound_message_start_time_.exchange(std::chrono::steady_clock::now());
            inbound_message_ = std::make_unique<Message>(version_, app_settings_.chain_config.value().magic_);
        }

        try {
            success_or_throw(inbound_message_->write(data));  // Note! data is consumed here

            msg_type = inbound_message_->get_type();
            success_or_throw(validate_message_for_protocol_handshake(DataDirectionMode::kInbound, msg_type));
            ASSERT(msg_type not_eq MessageType::kMissingOrUnknown and "Must have a valid message type");

            StopWatch deserialization_timer(/*auto_start=*/true);
            auto payload_ptr{MessagePayload::from_type(msg_type)};
            if (not payload_ptr) success_or_throw(Error::kMessagePayLoadUnhandleable);
            success_or_throw(payload_ptr->deserialize(inbound_message_->data()));
            const auto deserialization_duration{deserialization_timer.stop()};

            if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
                const std::list<std::string> log_params{"action",
                                                        __func__,
                                                        "command",
                                                        command_from_message_type(msg_type),
                                                        "size",
                                                        to_human_bytes(inbound_message_->data().size()),
                                                        "deserialization time",
                                                        StopWatch::format(deserialization_duration.second)};
                print_log(log::Level::kTrace, log_params);
            }

            if (not inbound_message_->data().eof()) success_or_throw(Error::kMessagePayloadExtraData);
            if (++messages_parsed > kMaxMessagesPerRead) success_or_throw(Error::kMessageFloodingDetected);

            // Deliver payload for local processing and eventually forward it to higher level code
            // And Reset the message barrel for the next message
            ASSERT(payload_ptr);
            success_or_throw(process_inbound_message(std::move(payload_ptr)));
            inbound_message_metrics_[msg_type].count_++;
            inbound_message_metrics_[msg_type].bytes_ += inbound_message_->data().size();
            inbound_message_.reset();
            inbound_message_start_time_.exchange(std::chrono::steady_clock::time_point::min());

        } catch (const boost::system::system_error& error) {
            if (error.code() == Error::kMessageHeaderIncomplete or error.code() == Error::kMessageBodyIncomplete) {
                // These two guys depict a normal condition: we must continue reading data from socket
                result = outcome::success();
                continue;
            }

            log::Warning("Node",
                         {"id", std::to_string(node_id_), "remote", to_string(), "action", __func__, "command",
                          command_from_message_type(msg_type, false), "status", "failure", "reason", error.what()});
            result = error.code();
            inbound_message_.reset();
            inbound_message_start_time_.exchange(std::chrono::steady_clock::time_point::min());
            break;
        }
    }

    return result;
}

outcome::result<void> Node::process_inbound_message(std::shared_ptr<MessagePayload> payload_ptr) {
    outcome::result<void> result{outcome::success()};
    std::string err_extended_reason{};
    bool notify_node_hub{false};

    ASSERT(payload_ptr not_eq nullptr);
    const auto msg_type{payload_ptr->type()};
    const auto command{command_from_message_type(msg_type)};
    switch (msg_type) {
        using enum MessageType;
        case kVersion: {
            auto& remote_version_payload = dynamic_cast<MsgVersionPayload&>(*payload_ptr);
            if (boost::algorithm::clamp(remote_version_payload.protocol_version_, kMinSupportedProtocolVersion,
                                        kMaxSupportedProtocolVersion) not_eq remote_version_payload.protocol_version_) {
                result = Error::kInvalidProtocolVersion;
                err_extended_reason =
                    absl::StrCat("Expected in range [", kMinSupportedProtocolVersion, ", ",
                                 kMaxSupportedProtocolVersion, "] got", remote_version_.protocol_version_, ".");
                break;
            }
            if (remote_version_payload.nonce_ == local_version_.nonce_) {
                result = Error::kConnectedToSelf;
                err_extended_reason = "Connected to self ? (same nonce)";
                break;
            }

            std::swap(remote_version_, remote_version_payload);
            version_.store(std::min(local_version_.protocol_version_, remote_version_.protocol_version_));
            const std::list<std::string> log_params{
                "agent",    remote_version_.user_agent_,
                "version",  std::to_string(remote_version_.protocol_version_),
                "services", std::to_string(remote_version_.services_),
                "relay",    (remote_version_.relay_ ? "true" : "false"),
                "block",    std::to_string(remote_version_.last_block_height_),
                "him",      remote_version_.sender_service_.endpoint_.to_string(),
                "me",       remote_version_.recipient_service_.endpoint_.to_string()};

            if (connection_ptr_->type_ == ConnectionType::kInbound and
                remote_version_.recipient_service_.endpoint_.is_routable()) {
                // TODO : SeenLocal() ?
            }

            // Should send our version message if not done already
            if (not(static_cast<uint32_t>(protocol_handshake_status_.load()) bitand
                    static_cast<uint32_t>(ProtocolHandShakeStatus::kVersionSent))) {
                std::ignore = push_message(local_version_, MessagePriority::kHigh);
            }
            print_log(log::Level::kInfo, log_params);
            std::ignore = push_message(MessageType::kVerAck, MessagePriority::kHigh);
            notify_node_hub = true;  // Will eventually set the outgoing address as good
        } break;
        case kVerAck:
            // This actually requires no action. Handshake flags already set
            // We don't need to forward the message elsewhere
            break;
        case kPing: {
            auto& ping_payload = dynamic_cast<MsgPingPongPayload&>(*payload_ptr);
            MsgPingPongPayload pong_payload(MessageType::kPong, ping_payload.nonce_);
            result = push_message(pong_payload, MessagePriority::kHigh);
        } break;
        case kGetAddr:
            if (connection_ptr_->type_ == ConnectionType::kInbound and inbound_message_metrics_[kGetAddr].count_ > 1U) {
                // Ignore the message to avoid fingerprinting
                err_extended_reason = "Ignoring duplicate 'getaddr' message on inbound connection.";
            } else {
                notify_node_hub = true;
            }
            break;
        case kPong: {
            auto& pong_payload = dynamic_cast<MsgPingPongPayload&>(*payload_ptr);
            const auto expected_nonce{ping_meter_.get_nonce()};
            if (not ping_meter_.pending_sample() or not expected_nonce.has_value()) {
                result = Error::kUnsolicitedPong;
                err_extended_reason = "Received an unrequested `pong` message.";
                break;
            }
            if (pong_payload.nonce_ not_eq expected_nonce.value()) {
                result = Error::kInvalidPingPongNonce;
                err_extended_reason =
                    absl::StrCat("Expected ", expected_nonce.value(), " got ", pong_payload.nonce_, ".");
                break;
            }
            ping_meter_.end_sample();
        } break;
        case kReject:
            // TODO : Decide how to handle
        default:
            // Notify node-hub of the inbound message which will eventually take ownership of it
            // As a result we can safely reset it which is done after this function returns
            notify_node_hub = true;
            break;
    }

    if (result.has_error() or not err_extended_reason.empty() or log::test_verbosity(log::Level::kTrace)) {
        const std::list<std::string> log_params{
            "action", __func__, "command",
            command,  "status", std::string(result.has_error() ? result.error().message() : "success")};
        print_log(result.has_error() ? log::Level::kWarning : log::Level::kTrace, log_params, err_extended_reason);
    }
    if (not result.has_error()) {
        if (msg_type not_eq MessageType::kPing and msg_type not_eq MessageType::kPong) {
            last_message_received_time_.exchange(std::chrono::steady_clock::now());
        }
        if (notify_node_hub) on_message_(shared_from_this(), std::move(payload_ptr));
    }
    payload_ptr.reset();
    return result;
}

outcome::result<void> Node::validate_message_for_protocol_handshake(const DataDirectionMode message_direction,
                                                                    const MessageType message_type) {
    using enum net::Error;

    /*
     * There's a mess in publicly available documentation about the protocol handshake correctness.
     *
     * According to https://en.bitcoin.it/wiki/Protocol_documentation#version the exchange of `version` message is
     * enough to allow peers begin exchange other message types (any) and `verack` is labelled as a simple
     * acknowledgement of the `version` message received. There's no constraint in `verack` message to be sent
     * immediately after `version` message.
     *
     * On the other hand https://developer.bitcoin.org/reference/p2p_networking.html#verack says that `verack` is
     * required to be sent by both peers (in response to a received `version`)  before any other message type
     * can be exchanged.
     * Quote "The “verack” message acknowledges a previously-received “version” message, informing the connecting
     * node that it can begin to send other messages."
     *
     * Apparently Bitcoin Core implementation as well as zcash and zend have opted for the more relaxed
     * former approach. For compatibility reasons we follow the same approach even if is in contrast with the logic
     * application of a proper handshake procedure.
     * In any case we must ensure that the exchange of `version` and `verack` messages is done only once per session,
     * in the correct order and in a time frame that is not too long (see kMaxProtocolHandshakeDurationSeconds).
     */

    const auto current_status{protocol_handshake_status_.load()};

    // We MUST begin with version exchange
    if (current_status == ProtocolHandShakeStatus::kNotInitiated and message_type != MessageType::kVersion) {
        return kInvalidProtocolHandShake;
    }

    // In case protocol handshake is completed any message type is allowed unless is a duplicate handshake message
    if (current_status == ProtocolHandShakeStatus::kCompleted) {
        if (message_type == MessageType::kVersion or message_type == MessageType::kVerAck) {
            return kDuplicateProtocolHandShake;
        }
        return outcome::success();
    }

    // Look for duplicates in the handshake sequence
    auto current_status_value = static_cast<uint32_t>(current_status);
    uint32_t new_status_flag{0U};
    switch (message_type) {
        using enum MessageType;
        case kVersion:
            new_status_flag = 1U << static_cast<uint32_t>(message_direction);
            if ((current_status_value & new_status_flag) == new_status_flag) return kDuplicateProtocolHandShake;
            break;
        case kVerAck:
            new_status_flag = 4U << static_cast<uint32_t>(message_direction);
            if ((current_status_value & new_status_flag) == new_status_flag) return kDuplicateProtocolHandShake;
            break;
        default:
            break;
    }

    // Apply the new status flag if the message pertains to the handshake sequence
    if (new_status_flag not_eq 0U) {
        const auto new_status{static_cast<ProtocolHandShakeStatus>(current_status_value bitor new_status_flag)};
        protocol_handshake_status_.exchange(new_status);
        if (new_status == ProtocolHandShakeStatus::kCompleted) {
            // Happens only once per session
            asio::post(io_strand_, [self{shared_from_this()}]() { self->on_handshake_completed(); });
        }
        return outcome::success();
    }

    // At least the `version` message must be exchanged before any other message type
    if (current_status_value < static_cast<uint32_t>(ProtocolHandShakeStatus::kVersionExchanged)) {
        return kInvalidProtocolHandShake;
    }

    // Replace the above with this if we want to be strict on handshake sequence
    //    if (current_status_value < static_cast<uint32_t>(ProtocolHandShakeStatus::kCompleted)) {
    //        return kInvalidProtocolHandShake;
    //    }

    return outcome::success();
}

void Node::on_handshake_completed() {
    if (not is_running()) return;

    // Lets' async_send out a ping immediately and start the timer
    // for subsequent pings
    const auto random_milliseconds{randomize<uint32_t>(app_settings_.network.ping_interval_seconds * 1'000U, 0.3)};
    auto ping_interval = std::chrono::milliseconds(random_milliseconds);
    on_ping_timer_expired(ping_interval);
    ping_timer_.autoreset(true);
    ping_timer_.start(ping_interval, [this](std::chrono::milliseconds& interval) { on_ping_timer_expired(interval); });

    // Unless this is an inbound connection, we must request addresses
    if (connection_ptr_->type_ not_eq ConnectionType::kInbound) {
        std::ignore = push_message(MessageType::kGetAddr);
    }
}

NodeIdleResult Node::is_idle() const noexcept {
    using enum NodeIdleResult;
    using namespace std::chrono;

    if (not is_running() or not connection_ptr_->socket_ptr_->is_open()) return kNotIdle;  // Not connected - not idle

    const auto now{steady_clock::now()};

    // Check whether we're waiting for a ping response
    if (const auto ping_duration{ping_meter_.pending_sample_duration().count()};
        ping_duration > app_settings_.network.ping_timeout_milliseconds) {
        const std::list<std::string> log_params{
            "action",  __func__,
            "status",  "ping timeout",
            "latency", absl::StrCat(ping_duration, "ms"),
            "max",     absl::StrCat(app_settings_.network.ping_timeout_milliseconds, "ms")};
        print_log(log::Level::kDebug, log_params, "Disconnecting ...");
        return kPingTimeout;
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

    // Check whether there's been any activity over the wire
    // Note ! Ping / Pong messages are not accounted
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

}  // namespace znode::net
