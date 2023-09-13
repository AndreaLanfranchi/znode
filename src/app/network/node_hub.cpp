/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <utility>

#include <absl/strings/str_cat.h>
#include <boost/asio/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <gsl/gsl_util>

#include <core/chain/seeds.hpp>
#include <core/common/assert.hpp>
#include <core/common/misc.hpp>

#include <app/common/log.hpp>
#include <app/network/node_hub.hpp>

namespace zenpp::network {

using namespace boost;
using asio::ip::tcp;

bool NodeHub::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started

    feed_connections_from_cli();
    feed_connections_from_dns();

    if (app_settings_.network.use_tls) {
        const auto ssl_data{(*app_settings_.data_directory)[DataDirectory::kSSLCert].path()};
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

    initialize_acceptor();
    info_stopwatch_.start(true);
    service_timer_.set_autoreset(true);
    service_timer_.start(250U, [this](unsigned interval) { return on_service_timer_expired(interval); });
    start_accept();
    return true;
}

bool NodeHub::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopping */ {
        socket_acceptor_.close();
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
        set_stopped();
    }
    return ret;
}

unsigned NodeHub::on_service_timer_expired(unsigned interval) {
    print_network_info();  // Print info every 5 seconds
    const bool running{is_running()};

    // If we have room connect to the pending connection requests
    if (running and size() < 64U /*app_settings_.network.max_active_connections*/ and
        not pending_connections_.empty()) {
        bool expected{false};
        if (async_connecting_.compare_exchange_strong(expected, true)) {
            const auto new_connection{pending_connections_.pop()};
            const std::lock_guard<std::mutex> lock{nodes_mutex_};
            if (not connected_addresses_.contains(*(*new_connection).endpoint_.address_)) {
                asio::post(asio_strand_, [this, new_connection]() { async_connect(*new_connection); });
            }
        }
    }

    const std::lock_guard<std::mutex> lock{nodes_mutex_};
    current_active_connections_.store(static_cast<uint32_t>(nodes_.size()));
    for (auto iterator{nodes_.begin()}; iterator not_eq nodes_.end(); /* !!! no increment !!! */) {
        if (iterator->second == nullptr) {
            iterator = nodes_.erase(iterator);
            continue;
        }
        if (iterator->second->status() == Node::ComponentStatus::kNotStarted) {
            on_node_disconnected(iterator->second);
            iterator->second.reset();
            continue;
        }
        if (not running) {
            std::ignore = iterator->second->stop(false);
            ++iterator;
            continue;
        }

        auto& node{*iterator->second};
        if (const auto idling_result{node.is_idle()}; idling_result not_eq NodeIdleResult::kNotIdle) {
            const std::string reason{magic_enum::enum_name(idling_result)};
            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "node",
                                     std::to_string(iterator->first), "remote", node.to_string(), "reason", reason})
                << "Disconnecting ...";
            std::ignore = node.stop(false);
        }
        ++iterator;
    }
    return interval;
}

void NodeHub::print_network_info() {
    // Let each cycle to last at least 5 seconds to have meaningful data and, of course, to avoid division by zero
    const auto info_lap_duration{
        gsl::narrow_cast<uint32_t>(duration_cast<std::chrono::seconds>(info_stopwatch_.since_start()).count())};
    if (info_lap_duration < 5) return;

    const auto current_total_bytes_received{total_bytes_received_.load()};
    const auto current_total_bytes_sent{total_bytes_sent_.load()};
    const auto period_total_bytes_received{current_total_bytes_received - last_info_total_bytes_received_.load()};
    const auto period_total_bytes_sent{current_total_bytes_sent - last_info_total_bytes_sent_.load()};

    std::vector<std::string> info_data;
    info_data.insert(info_data.end(), {"peers i/o", absl::StrCat(current_active_inbound_connections_.load(), "/",
                                                                 current_active_outbound_connections_.load())});
    info_data.insert(info_data.end(), {"data i/o", absl::StrCat(to_human_bytes(current_total_bytes_received, true), " ",
                                                                to_human_bytes(current_total_bytes_sent, true))});

    auto period_bytes_received_per_second{to_human_bytes(period_total_bytes_received / info_lap_duration, true) + "s"};
    auto period_bytes_sent_per_second{to_human_bytes(period_total_bytes_sent / info_lap_duration, true) + "s"};

    info_data.insert(info_data.end(),
                     {"speed i/o", period_bytes_received_per_second + " " + period_bytes_sent_per_second});

    last_info_total_bytes_received_.store(current_total_bytes_received);
    last_info_total_bytes_sent_.store(current_total_bytes_sent);

    log::Info("Network usage", info_data);
    info_stopwatch_.start(true);
}

void NodeHub::feed_connections_from_cli() {
    for (auto const& str : app_settings_.network.connect_nodes) {
        const IPConnection connection{IPEndpoint(str), IPConnectionType::kManualOutbound};
        std::ignore = pending_connections_.push(connection);
    }
}

void NodeHub::feed_connections_from_dns() {
    if (not app_settings_.network.force_dns_seeding and not pending_connections_.empty()) return;
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
            std::ignore = pending_connections_.push(IPConnection{endpoint, IPConnectionType::kSeedOutbound});
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

void NodeHub::async_connect(const IPConnection& connection) {
    // Final action: reset the flag
    auto reset_flag{gsl::finally([this]() {
        async_connecting_.exchange(false, std::memory_order_seq_cst);
        // std::atomic_thread_fence(std::memory_order_release);
        // async_connecting_.store(false, std::memory_order_relaxed);
    })};
    const std::string remote{connection.endpoint_.to_string()};

    log::Info("Service", {"name", "Node Hub", "action", "connect", "remote", remote});
    std::unique_lock<std::mutex> lock{nodes_mutex_};
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
    boost::asio::ip::tcp::socket socket{asio_context_};
    boost::system::error_code socket_error_code{asio::error::in_progress};
    const auto deadline{std::chrono::steady_clock::now() +
                        std::chrono::seconds(app_settings_.network.connect_timeout_seconds)};

    try {
        socket.async_connect(
            connection.endpoint_.to_endpoint(),
            [&socket_error_code](const boost::system::error_code& error_code) { socket_error_code = error_code; });

        while (socket_error_code == asio::error::in_progress) {
            // std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::this_thread::yield();
            if (std::chrono::steady_clock::now() > deadline) {
                std::ignore = socket.close(socket_error_code);
                socket_error_code = boost::asio::error::timed_out;
                break;
            }
            if (not is_running()) {
                std::ignore = socket.close(socket_error_code);
                socket_error_code = boost::asio::error::operation_aborted;
                break;
            }
        }

        if (socket_error_code) throw std::runtime_error(socket_error_code.message());
        set_common_socket_options(socket);
    } catch (const std::runtime_error& exception) {
        std::ignore = socket.close(socket_error_code);
        std::ignore = log::Error(
            "Service", {"name", "Node Hub", "action", "async_connect", "remote", remote, "error", exception.what()});
        return;
    } catch (...) {
        std::ignore = socket.close(socket_error_code);
        std::ignore = log::Error("Service",
                                 {"name", "Node Hub", "action", "async_connect", "remote", remote, "error", "unknown"});
        return;
    }

    if (not is_running()) return;
    std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, connection, asio_context_, std::move(socket), tls_client_context_.get(),
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
                on_node_received_message(node, std::move(message));
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);

    new_node->start();
    on_node_connected(new_node);
}

void NodeHub::start_accept() {
    if (not is_running()) return;
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service", {"name", "Node Hub", "status", "Listening for connections ..."});
    }
    socket_acceptor_.async_accept(
        [this](const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket) {
            handle_accept(error_code, std::move(socket));
        });
}

void NodeHub::handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket) {
    if (not is_running() or error_code == boost::asio::error::operation_aborted) {
        return;
    }

    boost::system::error_code local_error_code;
    const auto close_socket{[&local_error_code](boost::asio::ip::tcp::socket& socket) {
        std::ignore = socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, local_error_code);
        socket.close();
    }};

    if (error_code) {
        log::Error("Service", {"name", "Node Hub", "action", "accept", "error", error_code.message()});
        close_socket(socket);
        start_accept();
        return;
    }

    std::ignore = socket.non_blocking(true, local_error_code);
    if (local_error_code) {
        log::Error("Service", {"name", "Node Hub", "action", "handle", "error", local_error_code.message()});
        close_socket(socket);
        start_accept();
        return;
    }

    // Check we do not exceed the maximum number of connections
    if (size() >= app_settings_.network.max_active_connections) {
        ++total_rejected_connections_;
        log::Warning("Service", {"name", "Node Hub", "action", "accept", "error", "max active connections reached"});
        close_socket(socket);
        start_accept();
        return;
    }

    // Check we do not exceed the maximum number of connections per IP
    std::unique_lock<std::mutex> lock{nodes_mutex_};
    if (auto item = connected_addresses_.find(socket.remote_endpoint().address());
        item not_eq connected_addresses_.end()) {
        if (item->second >= app_settings_.network.max_active_connections_per_ip) {
            ++total_rejected_connections_;
            log::Warning("Service",
                         {"name", "Node Hub", "action", "accept", "error", "max active connections per ip reached"});
            close_socket(socket);
            start_accept();
            return;
        }
    }
    lock.unlock();

    set_common_socket_options(socket);
    const IPEndpoint remote{socket.remote_endpoint()};
    const IPEndpoint local{socket.local_endpoint()};
    const IPConnection connection(remote, IPConnectionType::kInbound);

    const std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, connection, asio_context_, std::move(socket), tls_server_context_.get(),
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
                on_node_received_message(node, std::move(message));
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);
    log::Info("Service", {"name", "Node Hub", "action", "accept", "local", local.to_string(), "remote",
                          remote.to_string(), "id", std::to_string(new_node->id())});

    new_node->start();
    on_node_connected(new_node);
    asio::post(asio_strand_, [this]() { this->start_accept(); });  // Continue listening for new connections
}

void NodeHub::initialize_acceptor() {
    IPEndpoint local_endpoint{app_settings_.network.local_endpoint};
    if (local_endpoint.port_ == 0)
        local_endpoint.port_ = gsl::narrow_cast<uint16_t>(app_settings_.chain_config->default_port_);

    socket_acceptor_.open(tcp::v4());
    socket_acceptor_.set_option(tcp::acceptor::reuse_address(true));
    socket_acceptor_.set_option(tcp::no_delay(true));
    socket_acceptor_.set_option(boost::asio::socket_base::keep_alive(true));
    socket_acceptor_.set_option(boost::asio::socket_base::receive_buffer_size(gsl::narrow_cast<int>(64_KiB)));
    socket_acceptor_.set_option(boost::asio::socket_base::send_buffer_size(gsl::narrow_cast<int>(64_KiB)));
    socket_acceptor_.bind(local_endpoint.to_endpoint());
    socket_acceptor_.listen();

    log::Info("Service", {"name", "Node Hub", "secure", (app_settings_.network.use_tls ? "yes" : "no"), "bound to",
                          local_endpoint.to_string()});
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
        using enum IPConnectionType;
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
            ASSERT(false);  // Should never happen
    }

    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "connections", std::to_string(total_connections_), "disconnections",
                    std::to_string(total_disconnections_), "rejections", std::to_string(total_rejected_connections_)});
    }
}

void NodeHub::on_node_connected(const std::shared_ptr<Node>& node) {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);
    connected_addresses_[*node->remote_endpoint().address_]++;
    ++total_connections_;
    switch (node->connection().type_) {
        case IPConnectionType::kInbound:
            ++current_active_inbound_connections_;
            break;
        case IPConnectionType::kOutbound:
        case IPConnectionType::kManualOutbound:
        case IPConnectionType::kSeedOutbound:
            ++current_active_outbound_connections_;
            break;
        default:
            ASSERT(false);  // Should never happen
    }
    nodes_.try_emplace(node->id(), node);
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service",
                   {"name", "Node Hub", "connections", std::to_string(total_connections_), "disconnections",
                    std::to_string(total_disconnections_), "rejections", std::to_string(total_rejected_connections_)});
    }
}

void NodeHub::on_node_data(network::DataDirectionMode direction, const size_t bytes_transferred) {
    switch (direction) {
        case DataDirectionMode::kInbound:
            total_bytes_received_ += bytes_transferred;
            break;
        case DataDirectionMode::kOutbound:
            total_bytes_sent_ += bytes_transferred;
            break;
        default:
            ASSERT(false);  // Should never happen
    }
}

void NodeHub::on_node_received_message(std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
    using namespace serialization;
    using enum Error;

    REQUIRES(message not_eq nullptr);
    REQUIRES(node not_eq nullptr);
    if (not is_running() or not node->is_running()) return;

    std::string error{};

    if (message->get_type() == abi::NetMessageType::kAddr) {
        abi::MsgAddrPayload addr_payload{};
        const auto ret{addr_payload.deserialize(message->data())};
        if (not ret) {
            // TODO Pass it to the address manager
            for (const auto& service : addr_payload.identifiers_) {
                if (app_settings_.network.ipv4_only and service.endpoint_.address_.get_type() == IPAddressType::kIPv6)
                    continue;
                if (app_settings_.chain_config->default_port_ != service.endpoint_.port_) {
                    log::Warning("Service", {"name", "Node Hub", "action", "handle_received_message[addr]", "remote",
                                             service.endpoint_.to_string(), "warn", "non standard port"});
                }
                std::ignore = pending_connections_.push(IPConnection{service.endpoint_, IPConnectionType::kOutbound});
            }
        } else {
            error = absl::StrCat("error ", magic_enum::enum_name(ret));
        }
    }

    if (not error.empty() or log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        const std::vector<std::string> log_params{
            "name",    "Node Hub",
            "action",  __func__,
            "command", std::string{magic_enum::enum_name(message->header().get_type())},
            "remote",  node->to_string(),
            "status",  error.empty() ? "success" : error};
        log::BufferBase((error.empty() ? log::Level::kTrace : log::Level::kError), "Service", log_params)
            << (error.empty() ? "" : "Disconnecting ...");
        if (not error.empty()) {
            asio::post(asio_strand_, [node]() { std::ignore = node->stop(false); });
        }
    }
}

std::shared_ptr<Node> NodeHub::operator[](int node_id) const {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);
    const auto iterator{nodes_.find(node_id)};
    if (iterator not_eq nodes_.end()) {
        return iterator->second;  // Might
    }
    return nullptr;
}

bool NodeHub::contains(int node_id) const {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);
    const auto iterator{nodes_.find(node_id)};
    return iterator not_eq nodes_.end() and iterator->second not_eq nullptr;
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
}  // namespace zenpp::network
