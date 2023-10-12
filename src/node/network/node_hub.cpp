/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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

namespace zenpp::net {

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
    asio::co_spawn(asio_context_, acceptor_work(), asio::detached);
    asio::co_spawn(asio_context_, connector_work(), asio::detached);
    asio::co_spawn(asio_context_, node_factory_work(), asio::detached);

    feed_connections_from_cli();
    feed_connections_from_dns();

    return true;
}

bool NodeHub::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopping */ {
        socket_acceptor_.close();
        pending_connections_.close();
        connection_requests_.close();
        // We MUST wait for all nodes to stop before returning otherwise
        // this instance falls out of scope and the nodes call a callback
        // which points to nowhere. The burden to stop nodes is on the
        // shoulders of the service timer.
        auto pending_nodes{size()};
        while (pending_nodes not_eq 0U) {
            log::Info("Service", {"name", "Node Hub", "action", "stop", "pending", std::to_string(pending_nodes)});
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            pending_nodes = size();
        }
        service_timer_.stop(true);
        info_timer_.stop(true);
        set_stopped();
    }
    return ret;
}

Task<void> NodeHub::node_factory_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "action", "node_factory_work", "status", "started"});
    while (is_running()) {
        boost::system::error_code error;
        auto conn_ptr = co_await pending_connections_.async_receive();
        if (conn_ptr->socket_ptr_ == nullptr) {
            std::ignore = log::Error("Service", {"name", "node_factory", "error", "null socket"});
            continue;
        }
        if (conn_ptr->type_ == ConnectionType::kNone) {
            std::ignore = log::Error("Service", {"name", "node_factory", "error", "invalid connection type"});
            std::ignore = conn_ptr->socket_ptr_->close(error);
            continue;
        }

        std::ignore = conn_ptr->socket_ptr_->close(error);
        LOG_INFO << "Got new socket and closed";
    }
    std::ignore = log::Trace("Service", {"name", "Node Hub", "action", "node_factory_work", "status", "stopped"});
}

Task<void> NodeHub::connector_work() {
    std::ignore = log::Trace("Service", {"name", "Node Hub", "action", "connector_work", "status", "started"});
    while (connection_requests_.is_open()) {
        // Poll channel for any outstanding connection request
        boost::system::error_code error;
        std::shared_ptr<Connection> conn_ptr;
        if (not connection_requests_.try_receive(conn_ptr)) {
            auto result = co_await connection_requests_.async_receive(error);
            if (error or not result.has_value()) continue;
            conn_ptr = result.value();
        }

        ASSERT_PRE(conn_ptr->socket_ptr_ == nullptr and "Socket must be null");
        std::ignore = log::Info("Service", {"name", "Node Hub", "action", "outgoing connection request", "remote",
                                            conn_ptr->endpoint_.to_string()});

        // Verify we're not exceeding connections per IP
        std::unique_lock lock{nodes_mutex_};
        if (const auto item = connected_addresses_.find(*conn_ptr->endpoint_.address_);
            item not_eq connected_addresses_.end() and
            item->second >= app_settings_.network.max_active_connections_per_ip) {
            log::Warning("Service", {"name", "Node Hub", "action", "outgoing connection request", "error",
                                     "same IP connections overflow"})
                << "Discarding ...";
            lock.unlock();
            continue;
        }
        lock.unlock();
    }
    std::ignore = log::Trace("Service", {"name", "Node Hub", "action", "connector_work", "status", "stopped"});
}

Task<void> NodeHub::acceptor_work() {
    try {
        initialize_acceptor();
        while (socket_acceptor_.is_open()) {
            auto socket_ptr = std::make_shared<boost::asio::ip::tcp::socket>(asio_context_);
            co_await socket_acceptor_.async_accept(*socket_ptr, boost::asio::use_awaitable);
            std::ignore = log::Info("Service", {"name", "Node Hub", "action", "incoming connection request", "remote",
                                                socket_ptr->remote_endpoint().address().to_string()});
            socket_ptr->non_blocking(true);

            IPEndpoint remote{socket_ptr->remote_endpoint()};
            auto conn_ptr = std::make_shared<Connection>(remote, ConnectionType::kInbound);
            conn_ptr->socket_ptr_ = std::move(socket_ptr);
            if (not pending_connections_.try_send(conn_ptr)) {
                co_await pending_connections_.async_send(std::move(conn_ptr));
            }
        }
    } catch (const boost::system::system_error& error) {
        if (error.code() not_eq boost::asio::error::operation_aborted) {
            log::Error("Service", {"name", "Node Hub", "action", "accept", "error", error.code().message()});
        }
    }

    std::ignore = log::Info("Service", {"name", "Node Hub", "action", "acceptor_work", "status", "stopped"});
    co_return;
}

// Task<void> NodeHub::accept_socket(boost::asio::ip::tcp::socket socket, IPConnection connection) {
//     if (not is_running()) co_return;
//
//     boost::system::error_code local_error_code;
//     const auto close_socket{[&local_error_code](boost::asio::ip::tcp::socket& _socket) {
//         std::ignore = _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, local_error_code);
//         _socket.close();
//     }};
//
//     // Check we do not exceed the maximum number of connections
//     if (size() >= app_settings_.network.max_active_connections) {
//         ++total_rejected_connections_;
//         log::Warning("Service", {"name", "Node Hub", "action", "accept", "error", "max active connections reached"});
//         close_socket(socket);
//         co_return;
//     }
//
//     // Check we do not exceed the maximum number of connections per IP
//     std::unique_lock lock{nodes_mutex_};
//     if (auto item = connected_addresses_.find(socket.remote_endpoint().address());
//         item not_eq connected_addresses_.end()) {
//         if (item->second >= app_settings_.network.max_active_connections_per_ip) {
//             ++total_rejected_connections_;
//             log::Warning("Service",
//                          {"name", "Node Hub", "action", "accept", "error", "max active connections per ip reached"});
//             close_socket(socket);
//             co_return;
//         }
//     }
//     lock.unlock();
//
//     set_common_socket_options(socket);
//     const auto new_node = std::make_shared<Node>(
//         app_settings_, connection, asio_context_, std::move(socket),
//         connection.type_ == IPConnectionType::kInbound ? tls_server_context_.get() : tls_client_context_.get(),
//         [this](DataDirectionMode direction, size_t bytes_transferred) { on_node_data(direction, bytes_transferred);
//         }, [this](std::shared_ptr<Node> node, std::shared_ptr<Message> message) {
//             on_node_received_message(std::move(node), std::move(message));
//         });
//
//     log::Info("Service", {"name", "Node Hub", "action", "accept", "remote", connection.endpoint_.to_string(), "id",
//                           std::to_string(new_node->id())});
//
//     new_node->start();
//     on_node_connected(new_node);
//     co_return;
// }

void NodeHub::on_service_timer_expired(std::chrono::milliseconds& /*interval*/) {
    const bool running{is_running()};
    std::unique_lock lock{nodes_mutex_};
    for (auto iterator{nodes_.begin()}; iterator not_eq nodes_.end(); /* !!! no increment !!! */) {
        if (*iterator == nullptr) {
            iterator = nodes_.erase(iterator);
            continue;
        } else if (not(*iterator)->is_running()) {
            iterator->reset();
            ++iterator;
            continue;
        }
        if (not running) {
            std::ignore = (*iterator)->stop(false);
            ++iterator;
            continue;
        }

        auto& node{*(*iterator)};
        if (const auto idling_result{node.is_idle()}; idling_result not_eq NodeIdleResult::kNotIdle) {
            const std::string reason{magic_enum::enum_name(idling_result)};
            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "remote",
                                     node.to_string(), "reason", reason})
                << "Disconnecting ...";
            std::ignore = node.stop(false);
        }
        ++iterator;
    }
    current_active_connections_.store(static_cast<uint32_t>(nodes_.size()));

    //    // If we have room connect to the pending connection requests
    //    if (running and nodes_.size() < 64U /*app_settings_.network.max_active_connections*/ and
    //        not pending_connections_.empty()) {
    //        bool expected{false};
    //        if (async_connecting_.compare_exchange_strong(expected, true)) {
    //            const auto new_connection{pending_connections_.pop()};
    //            if (not connected_addresses_.contains(*(*new_connection).endpoint_.address_)) {
    //                asio::post(asio_strand_, [this, new_connection]() { async_connect(*new_connection); });
    //            }
    //        }
    //    }
}

void NodeHub::on_info_timer_expired(std::chrono::milliseconds& interval) {
    using namespace std::chrono;
    static uint32_t lap_duration_seconds{0};
    lap_duration_seconds += gsl::narrow_cast<uint32_t>(duration_cast<seconds>(interval).count());
    if (lap_duration_seconds < 5) return;

    const auto current_total_bytes_received{total_bytes_received_.load()};
    const auto current_total_bytes_sent{total_bytes_sent_.load()};
    const auto period_total_bytes_received{current_total_bytes_received - last_info_total_bytes_received_.load()};
    const auto period_total_bytes_sent{current_total_bytes_sent - last_info_total_bytes_sent_.load()};

    std::vector<std::string> info_data;
    info_data.insert(info_data.end(), {"peers i/o", absl::StrCat(current_active_inbound_connections_.load(), "/",
                                                                 current_active_outbound_connections_.load())});
    info_data.insert(info_data.end(), {"data i/o", absl::StrCat(to_human_bytes(current_total_bytes_received, true), " ",
                                                                to_human_bytes(current_total_bytes_sent, true))});

    auto period_bytes_received_per_second{to_human_bytes(period_total_bytes_received / lap_duration_seconds, true) +
                                          "s"};
    auto period_bytes_sent_per_second{to_human_bytes(period_total_bytes_sent / lap_duration_seconds, true) + "s"};

    info_data.insert(info_data.end(),
                     {"speed i/o", period_bytes_received_per_second + " " + period_bytes_sent_per_second});

    last_info_total_bytes_received_.store(current_total_bytes_received);
    last_info_total_bytes_sent_.store(current_total_bytes_sent);

    std::ignore = log::Info("Network usage", info_data);
    lap_duration_seconds = 0;  // Accumulate for another 5 seconds
}

void NodeHub::feed_connections_from_cli() {
    for (auto const& str : app_settings_.network.connect_nodes) {
        const auto endpoint{IPEndpoint::from_string(str)};
        if (not endpoint) continue;  // Invalid endpoint - Should not happen as already validated by CLI
        auto conn_ptr = std::make_shared<Connection>(endpoint.value(), ConnectionType::kManualOutbound);
        std::ignore = connection_requests_.try_send(std::move(conn_ptr));
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
            std::ignore = connection_requests_.try_send(std::move(conn_ptr));
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

void NodeHub::async_connect(const Connection& connection) {
    const std::string remote{connection.endpoint_.to_string()};
    const auto reset_connecting{
        gsl::finally([this]() { async_connecting_.exchange(false, std::memory_order_seq_cst); })};

    log::Info("Service", {"name", "Node Hub", "action", "connect", "remote", remote});
    std::unique_lock lock{nodes_mutex_};
    if (auto item = connected_addresses_.find(*connection.endpoint_.address_);
        item not_eq connected_addresses_.end() and
        item->second >= app_settings_.network.max_active_connections_per_ip) {
        log::Error("Service",
                   {"name", "Node Hub", "action", "connect", "error", "max active connections per ip reached"});
        lock.unlock();
        return;
    }
    lock.unlock();

    // Create the socket and try to connect using a timeout
    auto socket_ptr = std::make_shared<boost::asio::ip::tcp::socket>(asio_context_);
    // boost::asio::ip::tcp::socket socket{asio_context_};
    boost::system::error_code socket_error_code{asio::error::in_progress};

    const auto deadline{std::chrono::steady_clock::now() +
                        std::chrono::seconds(app_settings_.network.connect_timeout_seconds)};

    try {
        socket_ptr->async_connect(
            connection.endpoint_.to_endpoint(),
            [&socket_error_code](const boost::system::error_code& error_code) { socket_error_code = error_code; });

        while (socket_error_code == asio::error::in_progress) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (std::chrono::steady_clock::now() > deadline) {
                std::ignore = socket_ptr->close(socket_error_code);
                socket_error_code = boost::asio::error::timed_out;
            } else if (not is_running()) {
                std::ignore = socket_ptr->close(socket_error_code);
                socket_error_code = boost::asio::error::operation_aborted;
            }
        }
        success_or_throw(socket_error_code);
        set_common_socket_options(*socket_ptr);
        auto conn_ptr = std::make_shared<Connection>(connection);
        conn_ptr->socket_ptr_ = std::move(socket_ptr);
        pending_connections_.try_send(std::move(conn_ptr));
        LOGF_INFO << "pending_connections_.try_send(std::move(socket_ptr));";
    } catch (const boost::system::system_error& error) {
        std::ignore = socket_ptr->close(socket_error_code);
        std::ignore = log::Error("Service", {"name", "Node Hub", "action", "async_connect", "remote", remote, "error",
                                             error.code().message()});
        return;
    }

    //    if (not is_running()) return;
    //    const auto new_node = std::make_shared<Node>(
    //        app_settings_, connection, asio_context_, std::move(socket), tls_client_context_.get(),
    //        [this](DataDirectionMode direction, size_t bytes_transferred) { on_node_data(direction,
    //        bytes_transferred); }, [this](std::shared_ptr<Node> node, std::shared_ptr<Message> message) {
    //            on_node_received_message(std::move(node), std::move(message));
    //        });
    //
    //    new_node->start();
    //    on_node_connected(new_node);
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

    log::Info("Service", {"name", "Node Hub", "secure", (app_settings_.network.use_tls ? "yes" : "no"), "listening on",
                          local_endpoint.value().to_string()});
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
            ASSERT(false && "Should not happen");
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
            ASSERT(false && "Should not happen");
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
            total_bytes_received_ += bytes_transferred;
            break;
        case kOutbound:
            total_bytes_sent_ += bytes_transferred;
            break;
        default:
            ASSERT(false && "Should not happen");
    }
}

void NodeHub::on_node_received_message(std::shared_ptr<Node> node, std::shared_ptr<Message> message) {
    using namespace ser;
    using enum Error;

    ASSERT_PRE(node not_eq nullptr);
    ASSERT_PRE(message not_eq nullptr and message->is_complete());
    if (not is_running() or not node->is_running()) return;

    const auto msg_type{message->get_type().value()};
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "action", __func__, "command", std::string{magic_enum::enum_name(msg_type)},
                    "remote", node->to_string(), "size", std::to_string(message->size())});
    }

    try {
        switch (msg_type) {
            using enum MessageType;
            case kAddr: {
                MsgAddrPayload addr_payload{};
                success_or_throw(addr_payload.deserialize(message->data()));
                // TODO Pass it to the address manager
                for (const auto& service : addr_payload.identifiers_) {
                    if (app_settings_.network.ipv4_only and
                        service.endpoint_.address_.get_type() == IPAddressType::kIPv6)
                        continue;
                    if (app_settings_.chain_config->default_port_ != service.endpoint_.port_) {
                        log::Warning("Service", {"name", "Node Hub", "action", __func__, "message", "addr", "entry",
                                                 service.endpoint_.to_string()})
                            << " << Non standard port";
                    }
                    std::ignore = connection_requests_.try_send(
                        std::make_shared<Connection>(service.endpoint_, ConnectionType::kOutbound));
                }
            } break;
            default:
                break;
        }

    } catch (const boost::system::system_error& error) {
        log::Error("Service", {"name", "Node Hub", "action", "on_node_received_message", "remote", node->to_string(),
                               "error", error.code().message()})
            << "Disconnecting ...";
        node->stop(false);
    } catch (const std::logic_error& error) {
        log::Error("Service", {"name", "Node Hub", "action", "on_node_received_message", "remote", node->to_string(),
                               "error", error.what()});
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
}  // namespace zenpp::net
