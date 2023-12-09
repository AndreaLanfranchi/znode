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

#include "node_hub.hpp"

#include <utility>

#include <absl/strings/str_cat.h>
#include <boost/asio/ssl.hpp>
#include <gsl/gsl_util>

#include <core/chain/seeds.hpp>
#include <core/common/assert.hpp>
#include <core/common/misc.hpp>

#include <infra/common/common.hpp>
#include <infra/common/log.hpp>
#include <infra/nat/detector.hpp>

namespace znode::net {

using namespace std::chrono_literals;
using namespace boost;
using asio::ip::tcp;

bool NodeHub::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started

    if (app_settings_.network.use_tls) {
        const auto ssl_data{(*app_settings_.data_directory)[DataDirectory::kSSLCertName].path()};
        auto* ctx{generate_tls_context(TLSContextType::kServer, ssl_data, app_settings_.network.tls_password)};
        if (ctx == nullptr) {
            log::Error("NodeHub", {"action", "start", "error", "failed to generate TLS server context"});
            return false;
        }
        tls_server_context_ = std::make_unique<asio::ssl::context>(ctx);
        ctx = generate_tls_context(TLSContextType::kClient, ssl_data, app_settings_.network.tls_password);
        if (ctx == nullptr) {
            log::Error("NodeHub", {"action", "start", "error", "failed to generate TLS client context"});
            return false;
        }
        tls_client_context_ = std::make_unique<asio::ssl::context>(ctx);
    }

    service_timer_.start(125ms, [this](std::chrono::milliseconds& interval) { on_service_timer_expired(interval); });
    info_timer_.start(5s, [this](std::chrono::milliseconds& interval) { on_info_timer_expired(interval); });

    // We need to determine our network address which will be used to advertise us to other nodes
    // If we have a NAT traversal option enabled we need to use the public address
    auto resolve_task = asio::co_spawn(asio_context_, nat::resolve(app_settings_.network.nat), asio::use_future);

    asio::co_spawn(asio_context_, node_factory_work(), asio::detached);
    asio::co_spawn(asio_context_, acceptor_work(), asio::detached);
    asio::co_spawn(asio_context_, connector_work(), asio::detached);
    asio::co_spawn(asio_context_, address_book_selector_work(), asio::detached);
    asio::co_spawn(asio_context_, address_book_processor_work(), asio::detached);

    resolve_task.wait();
    log::Info("Service", {"name", "Node Hub", "action", "start", "advertising address",
                          app_settings_.network.nat.address_.to_string()});

    feed_connections_from_cli();
    feed_connections_from_dns();

    return true;
}

bool NodeHub::stop() noexcept {
    const auto ret{Stoppable::stop()};
    if (ret) /* not already stopping */ {
        socket_acceptor_.close();
        node_factory_feed_.close();
        connector_feed_.close();
        need_connections_.close();
        address_book_processor_feed_.close();

        // We MUST wait for all nodes to stop before returning otherwise
        // this instance falls out of scope and the nodes call a callback
        // which points to nowhere. The burden to stop nodes is on the
        // shoulders of the service timer.
        auto pending_nodes{size()};
        while (pending_nodes not_eq 0U) {
            log::Info("Service", {"name", "Node Hub", "action", "stop", "pending", std::to_string(pending_nodes)});
            std::this_thread::sleep_for(std::chrono::seconds(2));
            pending_nodes = size();
        }

        service_timer_.stop();
        info_timer_.stop();
        set_stopped();
    }
    return ret;
}

Task<void> NodeHub::node_factory_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "node_factory", "status", "started"});

    while (node_factory_feed_.is_open()) {
        // Poll channel for any outstanding pending connection
        boost::system::error_code error;
        std::shared_ptr<Connection> conn_ptr;
        if (not node_factory_feed_.try_receive(conn_ptr)) {
            const auto& result = co_await node_factory_feed_.async_receive(error);
            if (error or not result.has_value()) continue;
            conn_ptr = result.value();
        }

        ASSERT_PRE(conn_ptr not_eq nullptr and conn_ptr->socket_ptr_ not_eq nullptr);
        ASSERT_PRE(conn_ptr->type_ not_eq ConnectionType::kNone);
        if (not conn_ptr->socket_ptr_->is_open()) continue;  // Remotely closed meanwhile ?

        boost::system::error_code local_error_code;
        const auto close_socket{[&local_error_code](tcp::socket& _socket) {
            std::ignore = _socket.shutdown(tcp::socket::shutdown_both, local_error_code);
            _socket.close();
        }};

        // Check we do not exceed the maximum number of connections
        if (size() >= app_settings_.network.max_active_connections) {
            ++total_rejected_connections_;
            log::Trace("Service", {"name", "Node Hub", "action", "accept", "error", "max active connections reached"});
            close_socket(*conn_ptr->socket_ptr_);
            continue;
        }

        const auto new_node = std::make_shared<Node>(
            app_settings_, conn_ptr, asio_context_,
            conn_ptr->type_ == ConnectionType::kInbound ? tls_server_context_.get() : tls_client_context_.get(),
            /* on_data */
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            /* on_message */
            [this](std::shared_ptr<Node> node_ptr, std::shared_ptr<MessagePayload> payload_ptr) {
                on_node_received_message(std::move(node_ptr), std::move(payload_ptr));
            },
            /* on_disconnected */
            [this](const Node& node) { on_node_disconnected(node); });

        log::Trace("Service", {"name", "Node Hub", "component", "nodefactory", "remote",
                               conn_ptr->endpoint_.to_string(), "id", std::to_string(new_node->id())});

        new_node->start();
        on_node_connected(new_node);
    }

    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "node_factory", "status", "stopped"});
    co_return;
}

Task<void> NodeHub::connector_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "connector", "status", "started"});

    while (is_running()) {
        // Poll channel for any queued address to connect to
        boost::system::error_code error;
        std::shared_ptr<Connection> conn_ptr;
        if (not connector_feed_.try_receive(conn_ptr)) {
            const auto result = co_await connector_feed_.async_receive(error);
            if (error or not result.has_value()) continue;
            conn_ptr = result.value();
        }
        if (!is_running()) break;

        auto prev_count = needed_connections_count_.load(std::memory_order_relaxed);
        if (prev_count > 0) needed_connections_count_.exchange(prev_count - 1);

        if (current_active_outbound_connections_ >= app_settings_.network.min_outgoing_connections) continue;
        const std::string remote{conn_ptr->endpoint_.to_string()};

        // Verify we're not exceeding connections per IP
        std::unique_lock lock{connected_addresses_mutex_};
        if (const auto item = connected_addresses_.find(*conn_ptr->endpoint_.address_);
            item not_eq connected_addresses_.end() and
            item->second >= app_settings_.network.max_active_connections_per_ip) {
            log::Trace("Service", {"name", "Node Hub", "action", "outgoing connection request", "remote", remote,
                                   "error", "same IP connections overflow"})
                << "Discarding ...";
            continue;
        }
        lock.unlock();

        try {
            log::Info("Service", {"name", "Node Hub", "remote", remote}) << "Connecting ...";
            co_await async_connect(*conn_ptr);
            std::ignore = address_book_.set_tried(conn_ptr->endpoint_);
        } catch (const boost::system::system_error& ex) {
            log::Warning("Service", {"name", "Node Hub", "action", "outgoing connection request", "remote", remote,
                                     "error", ex.code().message()});
            std::ignore = conn_ptr->socket_ptr_->close(error);
            // Unless operation have been aborted mark the address as failed
            if (ex.code() not_eq boost::asio::error::operation_aborted) {
                std::ignore = address_book_.set_failed(conn_ptr->endpoint_);
            }
            continue;
        }

        if (not node_factory_feed_.is_open()) break;
        if (not node_factory_feed_.try_send(conn_ptr)) {
            error.clear();
            co_await node_factory_feed_.async_send(error, std::move(conn_ptr));
            if (error) break;
        }
    }
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "connector", "status", "stopped"});
    co_return;
}

Task<void> NodeHub::address_book_selector_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "selector", "status", "started"});

    std::optional<IPAddressType> address_type;
    if (app_settings_.network.ipv4_only) {
        address_type = IPAddressType::kIPv4;
    }

    while (is_running()) {
        if (!need_connections_.notified()) {
            co_await need_connections_.wait_one();
        }
        if (not is_running() || !need_connections_.is_open()) break;

        // Pull as many addresses from the address book as required
        const auto needed_count = needed_connections_count_.load(std::memory_order_relaxed);
        for (uint32_t i{0}; i < needed_count; ++i) {
            if (not is_running() || !need_connections_.is_open()) break;
            auto [endpoint, last_tried]{address_book_.select_random(/*new_only=*/false, address_type)};
            if (not endpoint.has_value()) {
                std::ignore = needed_connections_count_.fetch_sub(1);
                continue;
            }
            auto conn_ptr = std::make_shared<Connection>(endpoint.value(), ConnectionType::kSeedOutbound);
            std::ignore = connector_feed_.try_send(std::move(conn_ptr));
        }
    }

    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "selector", "status", "stopped"});
    co_return;
}

Task<void> NodeHub::address_book_processor_work() {
    using namespace std::chrono_literals;
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "address book", "status", "started"});
    while (is_running()) {
        // Poll channel for any queued address to connect to
        boost::system::error_code error;
        NodeAndPayload item{nullptr, nullptr};
        if (not address_book_processor_feed_.try_receive(item)) {
            const auto result = co_await address_book_processor_feed_.async_receive(error);
            if (error or not result.has_value()) continue;
            item = result.value();
        }
        if (!is_running()) break;
        auto [node_ptr, payload_ptr] = std::move(item);
        if (node_ptr == nullptr or payload_ptr == nullptr) continue;

        try {
            switch (payload_ptr->type()) {
                case MessageType::kAddr: {
                    auto& payload = dynamic_cast<MsgAddrPayload&>(*payload_ptr);
                    payload.shuffle();
                    std::ignore = address_book_.add_new(payload.identifiers_, node_ptr->remote_endpoint().address_, 2h);
                } break;
                case MessageType::kGetAddr: {
                    // TODO : Implement
                } break;
                default:
                    ASSERT(false and "Should not happen");
            }
        } catch (const std::invalid_argument& ex) {
            log::Warning("Service", {"name", "Node Hub", "action", "address book", "error", ex.what()});
            node_ptr->stop();
            continue;
        }
    }
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "address book", "status", "stopped"});
    co_return;
}

Task<void> NodeHub::async_connect(Connection& connection) {
    const auto protocol = connection.endpoint_.address_.get_type() == IPAddressType::kIPv4 ? tcp::v4() : tcp::v6();
    connection.socket_ptr_ = std::make_shared<tcp::socket>(asio_context_);
    connection.socket_ptr_->open(protocol);

    /*
     * Calling async_connect() with a black-hole-routed destination resulted in the handler being called with
     * the error boost::asio::error::timed_out after ~380 seconds. (5 minutes !!!)
     *
     * This is due to this :
     * $ sysctl net.ipv4.tcp_syn_retries
     * net.ipv4.tcp_syn_retries = 6 (YMMV This is on WSL2)
     *
     * On connection attempt the initial SYN is sent and eventually there are up to 6 retries.
     * The first retry is sent after 3 seconds with the retry interval doubling every time.
     * 3 -> 6 -> 12 -> 24 -> 48 ...
     * See RFC1122 (4.2.3.5)
     *
     * As a result to effectively timeout at a reasonable value we must tamper with
     * the maximum number of retries.
     */

#ifndef _WIN32
    using syncnt_option_t = boost::asio::detail::socket_option::integer<IPPROTO_TCP, TCP_SYNCNT>;
    connection.socket_ptr_->set_option(syncnt_option_t(2));  // TODO adjust to CLI value
#else
    // Windows does not support TCP_SYNCNT use TCP_MAXRT instead
    // See https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
    using syncnt_option_t = boost::asio::detail::socket_option::integer<IPPROTO_TCP, TCP_MAXRT>;
    connection.socket_ptr_->set_option(syncnt_option_t(3));  // TODO adjust to CLI value
#endif

    co_await connection.socket_ptr_->async_connect(connection.endpoint_.to_endpoint(), boost::asio::use_awaitable);
    set_common_socket_options(*connection.socket_ptr_);
}

Task<void> NodeHub::acceptor_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "acceptor", "status", "started"});
    try {
        initialize_acceptor();
        while (socket_acceptor_.is_open()) {
            auto socket_ptr = std::make_shared<tcp::socket>(socket_acceptor_.get_executor());
            co_await socket_acceptor_.async_accept(*socket_ptr, boost::asio::use_awaitable);

            {
                auto log_obj = log::Info("Service", {"name", "Node Hub", "action", "incoming connection request",
                                                     "remote", socket_ptr->remote_endpoint().address().to_string()});
                if (size() >= app_settings_.network.max_active_connections) {
                    log_obj << "Rejected [max connections reached] ...";
                    boost::system::error_code error;
                    std::ignore = socket_ptr->close(error);
                    continue;
                }
            }

            socket_ptr->non_blocking(true);
            IPEndpoint remote{socket_ptr->remote_endpoint()};
            auto conn_ptr = std::make_shared<Connection>(remote, ConnectionType::kInbound);
            conn_ptr->socket_ptr_ = std::move(socket_ptr);
            if (not node_factory_feed_.try_send(conn_ptr)) {
                co_await node_factory_feed_.async_send(std::move(conn_ptr));
            }
        }
    } catch (const boost::system::system_error& error) {
        if (error.code() not_eq boost::asio::error::operation_aborted) {
            log::Error("Service", {"name", "Node Hub", "action", "accept", "error", error.code().message()});
        }
    }

    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "acceptor", "status", "stopped"});
    co_return;
}

void NodeHub::on_service_timer_expired(con::Timer::duration& /*interval*/) {
    const bool this_is_running{is_running()};

    std::unique_lock lock(nodes_mutex_, std::defer_lock);
    if (!lock.try_lock()) return;  // We'll defer to next timer tick

    // Randomly shutdown one node
    // TODO remove this when done testing
    uint32_t it_index{0};
    std::optional<uint32_t> random_index;
    if (this_is_running && nodes_.size() > 1 && randomize<uint32_t>(0U, 960U) == 0) {
        random_index.emplace(randomize<uint32_t>(0U, gsl::narrow_cast<uint32_t>(nodes_.size()) - 1));
    }

    for (auto iterator{nodes_.begin()}; iterator not_eq nodes_.end(); /* !!! no increment !!! */) {
        if (*iterator == nullptr) {
            iterator = nodes_.erase(iterator);
            continue;
        }

        if ((*iterator)->status() == ComponentStatus::kNotStarted) {
            if (static_cast<unsigned>(iterator->use_count()) == 1U) iterator->reset();
        } else if (not this_is_running) {
            if ((*iterator)->stop()) break;
        } else if (random_index and it_index == random_index.value() and
                   (*iterator)->connection().type_ not_eq ConnectionType::kInbound) {
            log::Info("Service", {"name", "Node Hub", "action", "handle_service_timer[shutdown]", "remote",
                                  (*iterator)->to_string()})
                << "Disconnecting ...";
            if ((*iterator)->stop()) break;
        } else if (const auto idling_result{(*iterator)->is_idle()}; idling_result not_eq NodeIdleResult::kNotIdle) {
            const std::string reason{magic_enum::enum_name(idling_result)};
            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "remote",
                                     (*iterator)->to_string(), "reason", reason})
                << "Disconnecting ...";
            if ((*iterator)->stop()) break;
        }
        ++iterator;
        ++it_index;
    }
    lock.unlock();
    if (not this_is_running) return;

    // Check whether we need to establish new connections
    if (needed_connections_count_ == 0 && not address_book_.empty() &&
        current_active_outbound_connections_ < app_settings_.network.min_outgoing_connections) {
        needed_connections_count_.exchange(app_settings_.network.min_outgoing_connections -
                                           current_active_outbound_connections_.load(std::memory_order_seq_cst));
        need_connections_.notify();
    }
}

void NodeHub::on_info_timer_expired(con::Timer::duration& /*interval*/) {
    std::vector<std::string> info_data;
    info_data.insert(info_data.end(), {"peers i/o", absl::StrCat(current_active_inbound_connections_.load(), "/",
                                                                 current_active_outbound_connections_.load())});

    const auto [new_buckets, tried_buckets]{address_book_.size_by_buckets()};
    info_data.insert(info_data.end(), {"addresses new/tried", absl::StrCat(new_buckets, "/", tried_buckets)});

    const auto [inbound_traffic, outbound_traffic]{traffic_meter_.get_cumulative_bytes()};
    info_data.insert(info_data.end(), {"traffic i/o", absl::StrCat(to_human_bytes(inbound_traffic, true), " ",
                                                                   to_human_bytes(outbound_traffic, true))});

    const auto [instant_speed_in, instant_speed_out]{traffic_meter_.get_interval_speed(true)};
    info_data.insert(info_data.end(), {"speed i/o", absl::StrCat(to_human_bytes(instant_speed_in, true), "s ",
                                                                 to_human_bytes(instant_speed_out, true), "s")});

    std::ignore = log::Info("Network usage", info_data);
}

void NodeHub::feed_connections_from_cli() {
    for (auto const& str : app_settings_.network.connect_nodes) {
        const auto endpoint{IPEndpoint::from_string(str)};
        if (not endpoint or not endpoint.value().is_routable()) continue;  // Invalid endpoint
        auto conn_ptr = std::make_shared<Connection>(endpoint.value(), ConnectionType::kManualOutbound);
        std::ignore = connector_feed_.try_send(std::move(conn_ptr));
    }
}

void NodeHub::feed_connections_from_dns() {
    if (not app_settings_.network.force_dns_seeding) return;
    const auto& hosts{get_chain_seeds(*app_settings_.chain_config)};
    std::map<std::string, std::vector<IPEndpoint>, std::less<>> host_to_endpoints{};

    // Lesson learned: when invoking the resolution of a hostname without additional parameters
    // the resolver will try IPv4 first and then IPv6. Problem is if an entry does not have an IPv4
    // address, the resolver will return immediately "host not found" without trying IPv6.
    // So we need to resolve the hostname for IPv4 and IPv6 separately.
    for (const auto& version : {tcp::v4(), tcp::v6()}) {
        if (app_settings_.network.ipv4_only and version == tcp::v6()) break;
        auto resolved_hosts{dns_resolve(hosts, version)};
        host_to_endpoints.merge(resolved_hosts);
    }

    for (const auto& [host_name, endpoints] : host_to_endpoints) {
        if (not is_running()) return;
        if (endpoints.empty()) {
            log::Error("NodeHub",
                       {"action", "dns_resolve", "host", host_name, "error", "Unable to resolve host or host unknown"});
            continue;
        }
        log::Info("NodeHub",
                  {"action", "dns_seeding", "host", host_name, "endpoints", std::to_string(endpoints.size())});
        for (const auto& endpoint : endpoints) {
            auto conn_ptr = std::make_shared<Connection>(endpoint, ConnectionType::kSeedOutbound);
            std::ignore = connector_feed_.try_send(std::move(conn_ptr));
        }
    }
}

std::map<std::string, std::vector<IPEndpoint>, std::less<>> NodeHub::dns_resolve(const std::vector<std::string>& hosts,
                                                                                 const tcp& version) {
    std::map<std::string, std::vector<IPEndpoint>, std::less<>> ret{};
    tcp::resolver resolver(asio_context_);
    const auto network_port = gsl::narrow_cast<uint16_t>(app_settings_.chain_config->default_port_);
    for (const auto& host : hosts) {
        if (not is_running()) break;
        boost::system::error_code error_code;
        const auto dns_entries{resolver.resolve(version, host, "", error_code)};
        if (error_code == boost::asio::error::host_not_found or error_code == boost::asio::error::no_data) continue;
        if (error_code) {
            log::Error("NodeHub", {"action", "dns_resolve", "host", host, "error", error_code.message()});
            continue;
        }
        // Push all results into the map
        std::ranges::transform(dns_entries, std::back_inserter(ret[host]), [network_port](const auto& result) {
            return IPEndpoint(result.endpoint().address(), network_port);
        });
    }
    return ret;
}

void NodeHub::initialize_acceptor() {
    auto local_endpoint{IPEndpoint::from_string(app_settings_.network.local_endpoint)};
    if (local_endpoint.has_error()) {
        throw boost::system::system_error(local_endpoint.error());
    }
    if (local_endpoint.value().port_ == 0)
        local_endpoint.value().port_ = gsl::narrow_cast<uint16_t>(app_settings_.chain_config->default_port_);

    socket_acceptor_.open(tcp::v4());
    socket_acceptor_.set_option(tcp::acceptor::reuse_address(true));
    socket_acceptor_.set_option(tcp::no_delay(true));
    socket_acceptor_.set_option(boost::asio::socket_base::keep_alive(true));
    socket_acceptor_.set_option(boost::asio::socket_base::receive_buffer_size(gsl::narrow_cast<int>(64_KiB)));
    socket_acceptor_.set_option(boost::asio::socket_base::send_buffer_size(gsl::narrow_cast<int>(64_KiB)));
    socket_acceptor_.bind(local_endpoint.value().to_endpoint());
    socket_acceptor_.listen();

    std::ignore = log::Info(
        "Service", {"name", "Node Hub", "component", "acceptor", "status", "listening", "endpoint",
                    local_endpoint.value().to_string(), "secure", (app_settings_.network.use_tls ? "yes" : "no")});
}

void NodeHub::on_node_connected(std::shared_ptr<Node> node_ptr) {
    {
        const std::unique_lock lock(connected_addresses_mutex_);
        connected_addresses_[*node_ptr->remote_endpoint().address_]++;
    }

    ++total_connections_;
    ++current_active_connections_;
    switch (node_ptr->connection().type_) {
        using enum ConnectionType;
        case kInbound:
            ++current_active_inbound_connections_;
            break;
        case kOutbound:
        case kManualOutbound:
        case kSeedOutbound:
            ++current_active_outbound_connections_;
            break;
        default:
            ASSERT(false and "Should not happen");
    }

    {
        const std::unique_lock lock(nodes_mutex_);
        nodes_.push_back(std::move(node_ptr));
    }
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "connections", std::to_string(total_connections_), "disconnections",
                    std::to_string(total_disconnections_), "rejections", std::to_string(total_rejected_connections_)});
    }
}

void NodeHub::on_node_disconnected(const Node& node) {
    std::unique_lock lock(connected_addresses_mutex_);
    if (auto item{connected_addresses_.find(*node.remote_endpoint().address_)};
        item not_eq connected_addresses_.end()) {
        if (--item->second == 0) {
            connected_addresses_.erase(item);
        }
    }
    lock.unlock();

    ++total_disconnections_;
    --current_active_connections_;
    switch (node.connection().type_) {
        using enum ConnectionType;
        case kInbound:
            --current_active_inbound_connections_;
            break;
        case kOutbound:
        case kManualOutbound:
        case kSeedOutbound:
            --current_active_outbound_connections_;
            break;
        default:
            ASSERT(false and "Should not happen");
    }

    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "connections", std::to_string(total_connections_), "disconnections",
                    std::to_string(total_disconnections_), "rejections", std::to_string(total_rejected_connections_)});
    }
}

void NodeHub::on_node_data(net::DataDirectionMode direction, const size_t bytes_transferred) {
    switch (direction) {
        using enum DataDirectionMode;
        case kInbound:
            traffic_meter_.update_inbound(bytes_transferred);
            break;
        case kOutbound:
            traffic_meter_.update_outbound(bytes_transferred);
            break;
        default:
            ASSERT(false and "Should not happen");
    }
}

void NodeHub::on_node_received_message(std::shared_ptr<Node> node_ptr, std::shared_ptr<MessagePayload> payload_ptr) {
    using namespace ser;
    using enum Error;

    ASSERT_PRE(node_ptr not_eq nullptr);
    ASSERT_PRE(payload_ptr not_eq nullptr);
    if (not is_running() or not node_ptr->is_running()) return;

    auto logger = log::Trace("Service", {"name", "Node Hub", "action", __func__, "remote", node_ptr->to_string(),
                                         "command", command_from_message_type(payload_ptr->type())});

    const auto msg_type{payload_ptr->type()};
    switch (msg_type) {
        using enum MessageType;
        case kVersion:
            if (node_ptr->connection().type_ != ConnectionType::kInbound) {
                std::ignore = address_book_.set_good(node_ptr->remote_endpoint());

                // Also send our address as advertisement
                MsgAddrPayload payload{};
                NodeService node_service{
                    IPEndpoint(app_settings_.network.nat.address_, app_settings_.chain_config->default_port_)};
                node_service.time_ = Now<NodeSeconds>();
                node_service.services_ = static_cast<uint64_t>(NodeServicesType::kNodeNetwork);
                payload.identifiers_.push_back(node_service);
                std::ignore = node_ptr->push_message(payload);
            }
            break;
        case kAddr:
        case kGetAddr:
            std::ignore =
                address_book_processor_feed_.try_send(std::make_pair(std::move(node_ptr), std::move(payload_ptr)));
            break;
        case kGetHeaders: {
            auto& payload = dynamic_cast<MsgGetHeadersPayload&>(*payload_ptr);
            logger << "items=" << std::to_string(payload.block_locator_hashes_.size());
        } break;
        case kInv: {
            auto& payload = dynamic_cast<MsgInventoryPayload&>(*payload_ptr);
            logger << "items=" << std::to_string(payload.items_.size());
        } break;
        default:
            break;
    }
}

size_t NodeHub::size() const { return current_active_connections_.load(); }

void NodeHub::set_common_socket_options(tcp::socket& socket) {
    timeval timeout{2, 0};
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));

    socket.set_option(tcp::no_delay(true));
    socket.set_option(tcp::socket::keep_alive(true));
    socket.set_option(boost::asio::socket_base::linger(true, 5));
    socket.set_option(boost::asio::socket_base::receive_buffer_size(gsl::narrow_cast<int>(64_KiB)));
    socket.set_option(boost::asio::socket_base::send_buffer_size(gsl::narrow_cast<int>(64_KiB)));
}
}  // namespace znode::net
