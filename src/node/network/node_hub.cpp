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

    service_timer_.start(250ms, [this](std::chrono::milliseconds& interval) { on_service_timer_expired(interval); });
    info_timer_.start(5s, [this](std::chrono::milliseconds& interval) { on_info_timer_expired(interval); });

    // We need to determine our network address which will be used to advertise us to other nodes
    // If we have a NAT traversal option enabled we need to use the public address
    auto resolve_task = asio::co_spawn(asio_context_, nat::resolve(app_settings_.network.nat), asio::use_future);

    asio::co_spawn(asio_context_, node_factory_work(), asio::detached);
    asio::co_spawn(asio_context_, acceptor_work(), asio::detached);
    asio::co_spawn(asio_context_, connector_work(), asio::detached);

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
        const auto close_socket{[&local_error_code](boost::asio::ip::tcp::socket& _socket) {
            std::ignore = _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, local_error_code);
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
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node_ptr, std::shared_ptr<MessagePayload> payload_ptr) {
                on_node_received_message(std::move(node_ptr), std::move(payload_ptr));
            });

        log::Info("Service", {"name", "Node Hub", "action", "accept", "remote", conn_ptr->endpoint_.to_string(), "id",
                              std::to_string(new_node->id())});

        new_node->start();
        on_node_connected(new_node);

        if (current_active_outbound_connections_ < app_settings_.network.min_outgoing_connections) {
            need_connections_.notify();
        }
    }

    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "node_factory", "status", "stopped"});
    co_return;
}

Task<void> NodeHub::connector_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "component", "connector", "status", "started"});

    need_connections_.notify();
    while (is_running()) {
        // Poll channel for any outstanding connection request
        boost::system::error_code error;
        std::shared_ptr<Connection> conn_ptr;
        if (not connector_feed_.try_receive(conn_ptr)) {
            const auto result = co_await connector_feed_.async_receive(error);
            if (error or not result.has_value()) continue;
            conn_ptr = result.value();
        }

        ASSERT_PRE(conn_ptr->socket_ptr_ == nullptr and "Socket must be null");
        if (app_settings_.network.ipv4_only and conn_ptr->endpoint_.address_.get_type() not_eq IPAddressType::kIPv4) {
            need_connections_.notify();
            continue;
        }

        const std::string remote{conn_ptr->endpoint_.to_string()};

        // Verify we're not exceeding connections per IP
        std::unique_lock lock{nodes_mutex_};
        if (const auto item = connected_addresses_.find(*conn_ptr->endpoint_.address_);
            item not_eq connected_addresses_.end() and
            item->second >= app_settings_.network.max_active_connections_per_ip) {
            log::Warning("Service", {"name", "Node Hub", "action", "outgoing connection request", "remote", remote,
                                     "error", "same IP connections overflow"})
                << "Discarding ...";
            lock.unlock();
            need_connections_.notify();
            continue;
        }
        lock.unlock();

        LOG_TRACE2 << "Connecting to " << remote;

        try {
            co_await async_connect(*conn_ptr);
        } catch (const boost::system::system_error& ex) {
            log::Warning("Service", {"name", "Node Hub", "action", "outgoing connection request", "remote", remote,
                                     "error", ex.code().message()});
            std::ignore = conn_ptr->socket_ptr_->close(error);
            need_connections_.notify();
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

Task<void> NodeHub::async_connect(Connection& connection) {
    const auto protocol = connection.endpoint_.address_.get_type() == IPAddressType::kIPv4 ? boost::asio::ip::tcp::v4()
                                                                                           : boost::asio::ip::tcp::v6();
    connection.socket_ptr_ = std::make_shared<boost::asio::ip::tcp::socket>(asio_context_);
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
            auto socket_ptr = std::make_shared<boost::asio::ip::tcp::socket>(socket_acceptor_.get_executor());
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
    const bool running{is_running()};
    size_t stopped_nodes{0};
    std::unique_lock lock{nodes_mutex_};
    for (auto iterator{nodes_.begin()}; iterator not_eq nodes_.end(); /* !!! no increment !!! */) {
        if (*iterator == nullptr) {
            iterator = nodes_.erase(iterator);
            continue;
        } else if (not(*iterator)->is_running()) {
            on_node_disconnected(*iterator);
            iterator->reset();
            ++iterator;
            continue;
        }
        if (not running) {
            std::ignore = (*iterator)->stop();
            if (++stopped_nodes == 16) break;  // Otherwise too many pending actions pile up.
            ++iterator;
            continue;
        }
        if (const auto idling_result{(*iterator)->is_idle()}; idling_result not_eq NodeIdleResult::kNotIdle) {
            const std::string reason{magic_enum::enum_name(idling_result)};
            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "remote",
                                     (*iterator)->to_string(), "reason", reason})
                << "Disconnecting ...";
            std::ignore = (*iterator)->stop();
        }
        ++iterator;
    }
    current_active_connections_.store(static_cast<uint32_t>(nodes_.size()));
}

void NodeHub::on_info_timer_expired(con::Timer::duration& /*interval*/) {
    std::vector<std::string> info_data;
    info_data.insert(info_data.end(), {"peers i/o", absl::StrCat(current_active_inbound_connections_.load(), "/",
                                                                 current_active_outbound_connections_.load())});

    const auto [inbound_traffic, outbound_traffic]{traffic_meter_.get_cumulative_bytes()};
    info_data.insert(info_data.end(), {"data i/o", absl::StrCat(to_human_bytes(inbound_traffic, true), " ",
                                                                to_human_bytes(outbound_traffic, true))});

    const auto [cumulative_speed_in, cumulative_speed_out]{traffic_meter_.get_cumulative_speed()};
    info_data.insert(info_data.end(),
                     {"overall speed i/o", absl::StrCat(to_human_bytes(cumulative_speed_in, true), "s ",
                                                        to_human_bytes(cumulative_speed_out, true), "s")});

    const auto [instant_speed_in, instant_speed_out]{traffic_meter_.get_interval_speed(true)};
    info_data.insert(info_data.end(),
                     {"instant speed i/o", absl::StrCat(to_human_bytes(instant_speed_in, true), "s ",
                                                        to_human_bytes(instant_speed_out, true), "s")});

    std::ignore = log::Info("Network usage", info_data);
}

void NodeHub::feed_connections_from_cli() {
    for (auto const& str : app_settings_.network.connect_nodes) {
        const auto endpoint{IPEndpoint::from_string(str)};
        if (not endpoint) continue;  // Invalid endpoint - Should not happen as already validated by CLI
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
    for (const auto& version : {boost::asio::ip::tcp::v4(), boost::asio::ip::tcp::v6()}) {
        if (app_settings_.network.ipv4_only and version == boost::asio::ip::tcp::v6()) break;
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
    boost::asio::ip::tcp::resolver resolver(asio_context_);
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

void NodeHub::on_node_disconnected(const std::shared_ptr<Node>& node) {
    if (auto item{connected_addresses_.find(*node->remote_endpoint().address_)};
        item not_eq connected_addresses_.end()) {
        if (--item->second == 0) {
            connected_addresses_.erase(item);
        }
    }

    ++total_disconnections_;
    switch (node->connection().type_) {
        using enum ConnectionType;
        case kInbound:
            if (current_active_inbound_connections_ not_eq 0U) {
                --current_active_inbound_connections_;
            }
            break;
        case kOutbound:
        case kManualOutbound:
        case kSeedOutbound:
            if (current_active_outbound_connections_ not_eq 0U) {
                --current_active_outbound_connections_;
            }
            break;
        default:
            ASSERT(false and "Should not happen");
    }
    current_active_connections_ = current_active_inbound_connections_ + current_active_outbound_connections_;
    if (current_active_outbound_connections_ < app_settings_.network.min_outgoing_connections) {
        need_connections_.notify();
    }
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "connections", std::to_string(total_connections_), "disconnections",
                    std::to_string(total_disconnections_), "rejections", std::to_string(total_rejected_connections_)});
    }
}

void NodeHub::on_node_connected(const std::shared_ptr<Node>& node) {
    const std::lock_guard lock(nodes_mutex_);
    connected_addresses_[*node->remote_endpoint().address_]++;
    ++total_connections_;
    switch (node->connection().type_) {
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
    nodes_.push_back(node);
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

    const auto msg_type{payload_ptr->type()};
    switch (msg_type) {
        using enum MessageType;
        case kAddr: {
            auto& payload = dynamic_cast<MsgAddrPayload&>(*payload_ptr);
            payload.shuffle();
            if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
                log::Trace("Service", {"name", "Node Hub", "action", __func__, "remote", node_ptr->to_string(),
                                       "message", "addr", "count", std::to_string(payload.identifiers_.size())})
                    << (log::test_verbosity(log::Level::kTrace2) ? payload.to_json().dump(4) : "");
            }

            // TODO Pass it to the address manager
            if (need_connections_.notified()) {
                for (const auto& service : payload.identifiers_) {
                    if (app_settings_.network.ipv4_only and
                        service.endpoint_.address_.get_type() == IPAddressType::kIPv6)
                        continue;
                    if (app_settings_.chain_config->default_port_ != service.endpoint_.port_) {
                        log::Debug("Service", {"name", "Node Hub", "action", __func__, "message", "addr", "entry",
                                               service.endpoint_.to_string()})
                            << " << Non standard port";
                    }
                    LOGF_TRACE << "Feeding connector with new address " << service.endpoint_.to_string();
                    std::ignore = connector_feed_.try_send(
                        std::make_shared<Connection>(service.endpoint_, ConnectionType::kOutbound));
                    break;
                }
            }
        } break;
        case kGetHeaders: {
            auto& payload = dynamic_cast<MsgGetHeadersPayload&>(*payload_ptr);
            if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
                log::Trace("Service",
                           {"name", "Node Hub", "action", __func__, "remote", node_ptr->to_string(), "message",
                            "getheaders", "count", std::to_string(payload.block_locator_hashes_.size())})
                    << (log::test_verbosity(log::Level::kTrace2) ? payload.to_json().dump(4) : "");
            }
        } break;
        case kInv: {
            auto& payload = dynamic_cast<MsgInventoryPayload&>(*payload_ptr);
            if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
                log::Trace("Service", {"name", "Node Hub", "action", __func__, "remote", node_ptr->to_string(),
                                       "message", "inv", "count", std::to_string(payload.items_.size())})
                    << (log::test_verbosity(log::Level::kTrace2) ? payload.to_json().dump(4) : "");
            }
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
