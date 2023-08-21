/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <list>

#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/misc.hpp>
#include <core/common/time.hpp>

#include <app/common/log.hpp>
#include <app/common/stopwatch.hpp>
#include <app/network/node.hpp>
#include <app/serialization/exceptions.hpp>

namespace zenpp::network {

using namespace boost;
using asio::ip::tcp;

std::atomic_int Node::next_node_id_{1};  // Start from 1 for user-friendliness

Node::Node(AppSettings& app_settings, NodeConnectionMode connection_mode, boost::asio::io_context& io_context,
           boost::asio::ip::tcp::socket socket, boost::asio::ssl::context* ssl_context,
           std::function<void(std::shared_ptr<Node>)> on_disconnect,
           std::function<void(DataDirectionMode, size_t)> on_data)
    : app_settings_(app_settings),
      connection_mode_(connection_mode),
      io_strand_(io_context),
      ping_timer_(io_context),
      socket_(std::move(socket)),
      ssl_context_(ssl_context),
      on_disconnect_(std::move(on_disconnect)),
      on_data_(std::move(on_data)) {
    // TODO Set version's services according to settings
    local_version_.version = kDefaultProtocolVersion;
    local_version_.services = static_cast<uint64_t>(NetworkServicesType::kNodeNetwork);
    local_version_.timestamp = time::unix_now();
    local_version_.addr_recv.address = socket_.remote_endpoint().address();
    local_version_.addr_recv.port = socket_.remote_endpoint().port();
    local_version_.addr_from.address = socket_.local_endpoint().address();
    local_version_.addr_from.port = 9033;  // TODO Set this value to the current listening port
    local_version_.nonce = app_settings_.network.nonce;
    local_version_.user_agent = get_buildinfo_string();
    local_version_.start_height = 0;  // TODO Set this value to the current blockchain height
    local_version_.relay = true;      // TODO
}

void Node::start() {
    if (bool expected{false}; !is_started_.compare_exchange_strong(expected, true)) {
        return;  // Already started
    }

    local_endpoint_ = socket_.local_endpoint();
    remote_endpoint_ = socket_.remote_endpoint();
    const auto now{std::chrono::steady_clock::now()};
    last_message_received_time_.store(now);  // We don't want to disconnect immediately
    last_message_sent_time_.store(now);      // We don't want to disconnect immediately
    connected_time_.store(now);

    if (ssl_context_ != nullptr) {
        ssl_stream_ = std::make_unique<asio::ssl::stream<tcp::socket&>>(socket_, *ssl_context_);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_ssl_handshake(); });
    } else {
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
        std::ignore = push_message(abi::NetMessageType::kVersion, local_version_);
    }

    start_ping_timer();
}

bool Node::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopped */ {
        is_connected_.store(false);
        is_writing_.store(false);
        ping_timer_.cancel();

        boost::system::error_code ec;
        if (ssl_stream_ != nullptr) {
            ssl_stream_->shutdown(ec);
            ssl_stream_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            ssl_stream_->lowest_layer().close(ec);
            // Don't reset the stream !!! There might be outstanding async operations
            // Let them gracefully complete
        } else {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        if (bool expected{true}; is_started_.compare_exchange_strong(expected, false)) {
            asio::post(io_strand_, [self{shared_from_this()}]() { self->on_disconnect_(self); });
        }
    }
    return ret;
}

void Node::start_ping_timer() {
    if (is_stopping()) return;

    // It would be boring to send out pings on a costant basis
    // So we randomize the interval a bit using a +/- 15% factor
    static const auto ping_interval_seconds{app_settings_.network.ping_interval_seconds};
    const auto ping_interval_seconds_min{ping_interval_seconds - ping_interval_seconds / 7};
    const auto ping_interval_seconds_max{ping_interval_seconds + ping_interval_seconds / 7};
    const auto ping_interval_seconds_randomized{
        randomize<uint32_t>(ping_interval_seconds_min, ping_interval_seconds_max)};

    ping_timer_.expires_after(std::chrono::seconds(ping_interval_seconds_randomized));
    ping_timer_.async_wait([self{shared_from_this()}](const boost::system::error_code& ec) {
        if (self->handle_ping_timer(ec)) {
            self->start_ping_timer();
        }
    });
}

bool Node::handle_ping_timer(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) return false;
    if (is_stopping()) return false;
    if (ping_nonce_.load()) return true;  // Wait for response to return
    last_ping_sent_time_.store(std::chrono::steady_clock::time_point::min());
    ping_nonce_.store(randomize<uint64_t>(uint64_t(/*min=*/1)));
    abi::PingPong ping{};
    ping.nonce = ping_nonce_.load();
    const auto ret{push_message(abi::NetMessageType::kPing, ping)};
    if (ret != serialization::Error::kSuccess) {
        const std::list<std::string> log_params{"action",  "ping",   "status",
                                                "failure", "reason", std::string(magic_enum::enum_name(ret))};
        print_log(log::Level::kError, log_params);
        stop(false);
        return false;
    }
    return true;
}

void Node::start_ssl_handshake() {
    REQUIRES(ssl_context_ != nullptr);
    if (!is_connected_.load() || !socket_.is_open()) return;
    asio::ssl::stream_base::handshake_type handshake_type{connection_mode_ == NodeConnectionMode::kInbound
                                                              ? asio::ssl::stream_base::server
                                                              : asio::ssl::stream_base::client};
    ssl_stream_->set_verify_mode(asio::ssl::verify_none);
    ssl_stream_->async_handshake(handshake_type, [self{shared_from_this()}](const boost::system::error_code& ec) {
        self->handle_ssl_handshake(ec);
    });
}

void Node::handle_ssl_handshake(const boost::system::error_code& ec) {
    if (ec) {
        const std::list<std::string> log_params{"action", "SSL handshake", "status", "failure", "reason", ec.message()};
        print_log(log::Level::kError, log_params, "Disconnecting ...");
        stop(true);
        return;
    }
    const std::list<std::string> log_params{"action", "SSL handshake", "status", "success"};
    print_log(log::Level::kInfo, log_params);
    start_read();
    push_message(abi::NetMessageType::kVersion, local_version_);
}

void Node::start_read() {
    if (is_stopping()) return;
    auto read_handler{[self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
        self->handle_read(ec, bytes_transferred);
    }};
    if (ssl_stream_) {
        ssl_stream_->async_read_some(receive_buffer_.prepare(kMaxBytesPerIO), read_handler);
    } else {
        socket_.async_read_some(receive_buffer_.prepare(kMaxBytesPerIO), read_handler);
    }
}

void Node::handle_read(const boost::system::error_code& ec, const size_t bytes_transferred) {
    // Due to the nature of asio, this function might be called late after stop() has been called
    // and the socket has been closed. In this case we should do nothing as the payload received (if any)
    // is not relevant anymore.
    if (is_stopping()) return;

    if (ec) [[unlikely]] {
        const std::list<std::string> log_params{"action", "handle_read", "status", "failure", "reason", ec.message()};
        print_log(log::Level::kError, log_params, "Disconnecting ...");
        stop(true);
        return;
    }

    if (bytes_transferred != 0) {
        receive_buffer_.commit(bytes_transferred);
        bytes_received_ += bytes_transferred;
        on_data_(DataDirectionMode::kInbound, bytes_transferred);

        const auto parse_result{parse_messages(bytes_transferred)};
        if (serialization::is_fatal_error(parse_result)) {
            const std::list<std::string> log_params{"action", "handle_read",
                                                    "status", "failure",
                                                    "reason", std::string(magic_enum::enum_name(parse_result))};
            print_log(log::Level::kError, log_params, "Disconnecting ...");
            stop(false);
            return;
        }
    }

    // Continue reading from socket
    asio::post(io_strand_, [self{shared_from_this()}]() { self->start_read(); });
}

void Node::start_write() {
    if (is_stopping()) return;
    if (bool expected{false}; !is_writing_.compare_exchange_strong(expected, true)) {
        return;  // Already writing - the queue will cause this to re-enter automatically
    }

    if (outbound_message_ && outbound_message_->data().eof()) {
        // A message has been fully sent
        // Unless it is a ping message we can reset the timer
        if (outbound_message_->get_type() != abi::NetMessageType::kPing) {
            last_message_sent_time_.store(std::chrono::steady_clock::now());
        }
        outbound_message_.reset();
        outbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
    }

    if (!outbound_message_) {
        // Try to get a new message from the queue
        std::scoped_lock lock{outbound_messages_mutex_};
        if (outbound_messages_.empty()) {
            is_writing_.store(false);
            return;  // Eventually next message submission to the queue will trigger a new write cycle
        }
        outbound_message_ = std::move(outbound_messages_.front());
        outbound_message_->data().seekg(0);
        outbound_messages_.erase(outbound_messages_.begin());
        outbound_message_start_time_.store(std::chrono::steady_clock::now());
    }

    // If message has been just loaded into the barrel then we must check its validity
    // against protocol handshake rules
    if (outbound_message_->data().tellg() == 0) {
        if (app_settings_.log.log_verbosity == log::Level::kTrace) {
            const std::list<std::string> log_params{
                "action",    "message",
                "direction", "out",
                "message",   std::string(magic_enum::enum_name(outbound_message_->get_type())),
                "size",      to_human_bytes(outbound_message_->data().size())};
            print_log(log::Level::kTrace, log_params);
        }

        auto error{
            validate_message_for_protocol_handshake(DataDirectionMode::kOutbound, outbound_message_->get_type())};
        if (error != serialization::Error::kSuccess) [[unlikely]] {
            if (app_settings_.log.log_verbosity >= log::Level::kError) {
                // TODO : Should we drop the connection here?
                // Actually outgoing messages' correct sequence is local responsibility
                // maybe we should either assert or push back the message into the queue
                const std::list<std::string> log_params{
                    "action",    "message",
                    "direction", "out",
                    "message",   std::string(magic_enum::enum_name(outbound_message_->get_type())),
                    "status",    "failure",
                    "reason",    std::string(magic_enum::enum_name(error))};
                print_log(log::Level::kError, log_params, "Disconnecting peer but is local fault ...");
            }
            outbound_message_.reset();
            is_writing_.store(false);
            stop(false);
            return;
        }

        // Should this be a ping message start timing the response
        if (outbound_message_->get_type() == abi::NetMessageType::kPing) {
            last_ping_sent_time_.store(std::chrono::steady_clock::now());
        }
    }

    // Push remaining data from the current message to the socket
    const auto bytes_to_write{std::min(kMaxBytesPerIO, outbound_message_->data().avail())};
    const auto data{outbound_message_->data().read(bytes_to_write)};
    REQUIRES(data);
    send_buffer_.sputn(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));

    auto write_handler{[self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
        self->handle_write(ec, bytes_transferred);
    }};
    if (ssl_stream_) {
        ssl_stream_->async_write_some(send_buffer_.data(), write_handler);
    } else {
        socket_.async_write_some(send_buffer_.data(), write_handler);
    }

    // We let handle_write to deal with re-entering the write cycle
}

void Node::handle_write(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (is_stopping()) {
        is_writing_.store(false);
        return;
    }

    if (ec) {
        if (app_settings_.log.log_verbosity >= log::Level::kError) {
            const std::list<std::string> log_params{"action",  "handle_write", "status",
                                                    "failure", "reason",       ec.message()};
            print_log(log::Level::kError, log_params, "Disconnecting ...");
        }
        stop(false);
        is_writing_.store(false);
        return;
    }

    if (bytes_transferred > 0) {
        bytes_sent_ += bytes_transferred;
        if (on_data_) on_data_(DataDirectionMode::kOutbound, bytes_transferred);
        send_buffer_.consume(bytes_transferred);
    }

    // If we have sent the whole message then we can start sending the next chunk
    if (send_buffer_.size()) {
        auto write_handler{
            [self{shared_from_this()}](const boost::system::error_code& ec, const size_t bytes_transferred) {
                self->handle_write(ec, bytes_transferred);
            }};
        if (ssl_stream_) {
            ssl_stream_->async_write_some(send_buffer_.data(), write_handler);
        } else {
            socket_.async_write_some(send_buffer_.data(), write_handler);
        }
    } else {
        is_writing_.store(false);
        asio::post(io_strand_, [self{shared_from_this()}]() { self->start_write(); });
    }
}

serialization::Error Node::push_message(const abi::NetMessageType message_type, serialization::Serializable& payload) {
    using namespace serialization;
    using enum Error;

    auto new_message = std::make_unique<abi::NetMessage>(version_);
    auto err{new_message->push(message_type, payload, app_settings_.network.magic_bytes)};
    if (!!err) {
        if (app_settings_.log.log_verbosity >= log::Level::kError) {
            const std::list<std::string> log_params{"action",  "push_message", "status",
                                                    "failure", "reason",       std::string{magic_enum::enum_name(err)}};
            print_log(log::Level::kError, log_params);
        }
        return err;
    }
    std::unique_lock lock(outbound_messages_mutex_);
    outbound_messages_.emplace_back(new_message.release());
    lock.unlock();

    auto self{shared_from_this()};
    boost::asio::post(io_strand_, [self]() { self->start_write(); });
    return kSuccess;
}

serialization::Error Node::push_message(const abi::NetMessageType message_type) {
    abi::NullData null_payload{};
    return push_message(message_type, null_payload);
}

void Node::begin_inbound_message() {
    inbound_message_ = std::make_unique<abi::NetMessage>(version_);
    inbound_message_start_time_.store(std::chrono::steady_clock::now());
}

void Node::end_inbound_message() {
    inbound_message_.reset();
    inbound_message_start_time_.store(std::chrono::steady_clock::time_point::min());
}

serialization::Error Node::parse_messages(const size_t bytes_transferred) {
    using namespace serialization;
    using enum Error;
    Error err{kSuccess};

    size_t messages_parsed{0};
    ByteView data{boost::asio::buffer_cast<const uint8_t*>(receive_buffer_.data()), bytes_transferred};
    while (!data.empty()) {
        if (!inbound_message_) begin_inbound_message();
        err = inbound_message_->parse(data, app_settings_.network.magic_bytes);  // Consumes data
        if (err == kMessageHeaderIncomplete) break;
        if (is_fatal_error(err)) {
            // Some debugging before exiting for fatal
            if (app_settings_.log.log_verbosity >= log::Level::kDebug) {
                switch (err) {
                    using enum Error;
                    case kMessageHeaderUnknownCommand:
                        print_log(
                            log::Level::kDebug,
                            {"action", "parse_messages", "unknown command",
                             std::string(reinterpret_cast<char*>(inbound_message_->header().command.data()), 12)});
                        break;
                    default:
                        break;
                }
            }
            break;
        }

        if (const auto err_handshake =
                validate_message_for_protocol_handshake(DataDirectionMode::kInbound, inbound_message_->get_type());
            err_handshake != kSuccess) {
            err = err_handshake;
            break;
        }

        if (err == kMessageBodyIncomplete) break;  // Can't do anything but read other data
        if (++messages_parsed > kMaxMessagesPerRead) {
            err = KMessagesFloodingDetected;
            break;
        }

        if (err = process_inbound_message(); err != kSuccess) break;
        end_inbound_message();
    }
    receive_buffer_.consume(bytes_transferred);
    if (!is_fatal_error(err) && messages_parsed != 0) {
        last_message_received_time_.store(std::chrono::steady_clock::now());
    }
    return err;
}

serialization::Error Node::process_inbound_message() {
    using namespace serialization;
    using enum Error;
    Error err{kSuccess};

    REQUIRES(inbound_message_ != nullptr);

    if (app_settings_.log.log_verbosity == log::Level::kTrace) [[unlikely]] {
        const std::list<std::string> log_params{
            "action",    "message",
            "direction", "in ",
            "message",   std::string{magic_enum::enum_name(inbound_message_->get_type())},
            "size",      to_human_bytes(inbound_message_->size())};
        print_log(log::Level::kTrace, log_params);
    }

    switch (inbound_message_->get_type()) {
        using enum abi::NetMessageType;
        case kVersion:
            if (err = remote_version_.deserialize(inbound_message_->data()); err != kSuccess) break;
            if (remote_version_.version < kMinSupportedProtocolVersion ||
                remote_version_.version > kMaxSupportedProtocolVersion) {
                err = kInvalidProtocolVersion;
                const std::list<std::string> log_params{
                    "action",    "message",
                    "direction", "in ",
                    "message",   std::string{magic_enum::enum_name(inbound_message_->get_type())},
                    "size",      to_human_bytes(inbound_message_->size()),
                    "status",    "failure",
                    "reason",    std::string(magic_enum::enum_name(err))};
                std::string extended_reason{"got " + std::to_string(remote_version_.version) +
                                            " but supported versions are " +
                                            std::to_string(kMinSupportedProtocolVersion) + " to " +
                                            std::to_string(kMaxSupportedProtocolVersion)};
                print_log(log::Level::kTrace, log_params, extended_reason);
                break;
            }
            {
                version_.store(std::min(local_version_.version, remote_version_.version));
                const std::list<std::string> log_params{
                    "agent",    remote_version_.user_agent,
                    "version",  std::to_string(remote_version_.version),
                    "nonce",    std::to_string(remote_version_.nonce),
                    "services", std::to_string(remote_version_.services),
                    "relay",    (remote_version_.relay ? "true" : "false"),
                    "block",    std::to_string(remote_version_.start_height),
                    "him",      network::to_string(remote_version_.addr_from.to_endpoint()),
                    "me",       network::to_string(remote_version_.addr_recv.to_endpoint())};
                if (remote_version_.nonce != local_version_.nonce) {
                    print_log(log::Level::kInfo, log_params);
                    err = push_message(abi::NetMessageType::kVerack);
                } else {
                    print_log(log::Level::kWarning, log_params, "Connected to self. Disconnecting ...");
                    asio::post(io_strand_, [self{shared_from_this()}]() { self->stop(false); });
                }
            }
            break;
        case kVerack:
            // If remote appears to be ahead of us request headers
            {
                abi::GetHeaders payload{};
                payload.version = version_.load();
                auto genesis_hash{h256::from_hex("0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602",
                                                 /*reverse=*/true)};
                payload.block_locator_hashes.push_back(*genesis_hash);
                err = push_message(abi::NetMessageType::kGetheaders, payload);
            }
            break;
        case kPing: {
            abi::PingPong ping_pong{};
            if (err = ping_pong.deserialize(inbound_message_->data()); err != kSuccess) break;
            err = push_message(abi::NetMessageType::kPong, ping_pong);
        } break;
        case kPong: {
            abi::PingPong ping_pong{};
            if (err = ping_pong.deserialize(inbound_message_->data()); err != kSuccess) break;
            if (ping_pong.nonce != ping_nonce_.load()) {
                err = kMismatchingPingPongNonce;
                const std::list<std::string> log_params{
                    "action",    "message",
                    "direction", "in ",
                    "message",   std::string{magic_enum::enum_name(inbound_message_->get_type())},
                    "size",      to_human_bytes(inbound_message_->size()),
                    "status",    "failure",
                    "reason",    std::string(magic_enum::enum_name(err))};
                print_log(log::Level::kTrace, log_params);
                break;
            }
            // Calculate ping response time
            const auto now{std::chrono::steady_clock::now()};
            const auto ping_response_time{now - last_ping_sent_time_.load()};
            print_log(log::Level::kInfo, {"action", "ping", "response time", StopWatch::format(ping_response_time)});

            ping_nonce_.store(0);
            last_ping_sent_time_.store(std::chrono::steady_clock::time_point::min());
        } break;
        default:
            std::scoped_lock lock{inbound_messages_mutex_};
            inbound_messages_.push_back(std::move(inbound_message_));
            // TODO notify higher level consumer(s)
    }

    return err;
}

serialization::Error Node::validate_message_for_protocol_handshake(const DataDirectionMode direction,
                                                                   const abi::NetMessageType message_type) {
    using namespace zenpp::abi;
    using enum serialization::Error;

    if (protocol_handshake_status_ != ProtocolHandShakeStatus::kCompleted) [[unlikely]] {
        // Only these two guys during handshake
        // See the switch below for details about the order
        if (message_type != NetMessageType::kVersion && message_type != NetMessageType::kVerack)
            return kInvalidProtocolHandShake;

        auto status{static_cast<uint32_t>(protocol_handshake_status_.load())};
        switch (direction) {
            using enum DataDirectionMode;
            using enum ProtocolHandShakeStatus;
            case kInbound:
                if (message_type == NetMessageType::kVersion &&
                    ((status & static_cast<uint32_t>(kRemoteVersionReceived)) ==
                     static_cast<uint32_t>(kRemoteVersionReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    ((status & static_cast<uint32_t>(kLocalVersionAckReceived)) ==
                     static_cast<uint32_t>(kLocalVersionAckReceived))) {
                    return kDuplicateProtocolHandShake;
                }
                status |= static_cast<uint32_t>(message_type == NetMessageType::kVersion ? kRemoteVersionReceived
                                                                                         : kLocalVersionAckReceived);
                break;
            case kOutbound:
                if (message_type == NetMessageType::kVersion &&
                    ((status & static_cast<uint32_t>(kLocalVersionSent)) == static_cast<uint32_t>(kLocalVersionSent))) {
                    return kDuplicateProtocolHandShake;
                }
                if (message_type == NetMessageType::kVerack &&
                    ((status & static_cast<uint32_t>(kRemoteVersionAckSent)) ==
                     static_cast<uint32_t>(kRemoteVersionAckSent))) {
                    return kDuplicateProtocolHandShake;
                }
                status |= static_cast<uint32_t>(message_type == NetMessageType::kVersion ? kLocalVersionSent
                                                                                         : kRemoteVersionAckSent);
                break;
        }
        protocol_handshake_status_.store(static_cast<ProtocolHandShakeStatus>(status));
    } else {
        switch (message_type) {
            using enum NetMessageType;
            case kVersion:  // None of these two is allowed
            case kVerack:   // after completed handshake
                return kDuplicateProtocolHandShake;
            default:
                break;
        }
    }
    return kSuccess;
}

void Node::clean_up(Node* ptr) noexcept {
    if (ptr) {
        ptr->stop(true);
        delete ptr;
    }
}

NodeIdleResult Node::is_idle() const noexcept {
    using enum NodeIdleResult;

    if (!is_connected()) return kNotIdle;  // Not connected - not idle
    using namespace std::chrono;
    const auto now{steady_clock::now()};

    // Check whether we're waiting for a ping response
    if (ping_nonce_.load() != 0) {
        const auto ping_duration{duration_cast<milliseconds>(now - last_ping_sent_time_.load()).count()};
        if (ping_duration > app_settings_.network.ping_timeout_milliseconds) {
            return kPingTimeout;
        }
    }

    // Check we've at least completed protocol handshake in a reasonable time
    if (protocol_handshake_status_.load() != ProtocolHandShakeStatus::kCompleted) {
        if (const auto connected_time{connected_time_.load()};
            static_cast<uint32_t>(duration_cast<seconds>(now - connected_time).count()) >
            app_settings_.network.protocol_handshake_timeout_seconds) {
            return kProtocolHandshakeTimeout;
        }
    }

    // Check whether there's an inbound message in progress
    if (const auto value{inbound_message_start_time_.load()}; value != steady_clock::time_point::min()) {
        const auto inbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (inbound_message_duration > app_settings_.network.inbound_timeout_seconds) {
            return kInboundTimeout;
        }
    }

    // Check whether there's an outbound message in progress
    if (const auto value{outbound_message_start_time_.load()}; value != steady_clock::time_point::min()) {
        const auto outbound_message_duration{duration_cast<seconds>(now - value).count()};
        if (outbound_message_duration > app_settings_.network.outbound_timeout_seconds) {
            return kOutboundTimeout;
        }
    }

    // Check whether there's been any activity
    const auto most_recent_activity_time{std::max(last_message_received_time_.load(), last_message_sent_time_.load())};
    const auto idle_seconds{duration_cast<seconds>(now - most_recent_activity_time).count()};
    if (static_cast<uint32_t>(idle_seconds) >= app_settings_.network.idle_timeout_seconds) {
        return kGlobalTimeout;
    }

    return kNotIdle;
}

std::string Node::to_string() const noexcept { return network::to_string(remote_endpoint_); }

void Node::print_log(const log::Level severity, const std::list<std::string>& params,
                     std::string extra_data) const noexcept {
    if (app_settings_.log.log_verbosity < severity) return;
    std::vector<std::string> log_params{"id", std::to_string(node_id_), "remote", this->to_string()};
    log_params.insert(log_params.end(), params.begin(), params.end());
    log::BufferBase(severity, "Node", log_params) << extra_data;
}

}  // namespace zenpp::network
