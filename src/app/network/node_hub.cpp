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
    service_timer_.start(kServiceTimerIntervalSeconds_ * 1'000U,
                         [this](unsigned interval) { return on_service_timer_expired(interval); });
    start_accept();
    return true;
}

bool NodeHub::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopping */ {
        socket_acceptor_.close();
        std::unique_lock<std::mutex> lock{nodes_mutex_};
        auto pending_nodes{nodes_.size()};
        std::ranges::for_each(nodes_, [](const auto& key_value) {
            if (key_value.second not_eq nullptr) {
                std::ignore = key_value.second->stop(true);
            }
        });
        lock.unlock();

        // We MUST wait for all nodes to stop before returning otherwise
        // this instance falls out of scope and the nodes call a callback
        // which points to nowhere
        while (pending_nodes not_eq 0U) {
            log::Info("Service", {"name", "Node Hub", "action", "stop", "pending", std::to_string(pending_nodes)});
            std::this_thread::sleep_for(std::chrono::seconds(1));
            pending_nodes = size();
        }

        service_timer_.stop(true);
    }
    return ret;
}

unsigned NodeHub::on_service_timer_expired(unsigned interval) {
    print_info();  // Print info every 5 seconds

    // If we have room connect to the pending connection requests
    if (not is_stopping() and size() < app_settings_.network.max_active_connections and
        not pending_connections_.empty() and not async_connecting_.load()) {
        const auto new_connection{pending_connections_.pop()};
        async_connecting_.store(true);
        asio::post(asio_strand_, [this, new_connection]() { async_connect(*new_connection); });
    }

    const std::lock_guard<std::mutex> lock{nodes_mutex_};
    for (auto iterator{nodes_.begin()}; iterator not_eq nodes_.end(); /* !!! no increment !!! */) {
        if (iterator->second == nullptr) {
            iterator = nodes_.erase(iterator);
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
    //    for (auto& [node_id, node_ptr] : nodes_) {
    //        if (is_stopping()) break;
    //        if (const auto result{node_ptr->is_idle()}; result not_eq NodeIdleResult::kNotIdle) {
    //            const std::string reason{magic_enum::enum_name(result)};
    //            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "node",
    //                                     std::to_string(node_id), "remote", node_ptr->to_string(), "reason", reason})
    //                << "Disconnecting ...";
    //
    // #if defined(__clang__) and __clang_major__ <= 15
    //            // Workaround for clang <=15 bug
    //            // cause structured bindings are allowed by C++20 to be captured in lambdas
    //            auto node_ptr_copy{node_ptr};
    //            asio::post(asio_strand_, [node_ptr_copy]() { std::ignore = node_ptr_copy->stop(false); });
    // #else
    //            asio::post(asio_strand_, [node_ptr_copy{node_ptr}]() { std::ignore = node_ptr_copy->stop(false); });
    // #endif
    //        }
    //    }
    return interval;
}

void NodeHub::print_info() {
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
        if (is_stopping()) return;
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
        if (is_stopping()) break;
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
    const auto release_connecting_flag{gsl::finally([this]() { async_connecting_.store(false); })};
    if (is_stopping()) return;

    const std::string remote{connection.endpoint_.to_string()};

    log::Info("Service", {"name", "Node Hub", "action", "connect", "remote", remote});
    if (size() >= app_settings_.network.max_active_connections) {
        log::Error("Service", {"name", "Node Hub", "action", "connect", "error", "max active connections reached"});
        return;
    }
    {
        const std::lock_guard<std::mutex> lock{nodes_mutex_};
        if (auto item = connected_addresses_.find(*connection.endpoint_.address_);
            item not_eq connected_addresses_.end() and
            item->second >= app_settings_.network.max_active_connections_per_ip) {
            log::Error("Service",
                       {"name", "Node Hub", "action", "connect", "error", "max active connections per ip reached"});
            return;
        }
    }

    // Create the socket and try connect using a timeout
    boost::asio::ip::tcp::socket socket{asio_context_};
    boost::system::error_code socket_error_code;
    auto connect_future{std::async(std::launch::async, [&socket, &connection, &socket_error_code]() {
        socket.connect(connection.endpoint_.to_endpoint(), socket_error_code);
    })};
    if (connect_future.wait_for(std::chrono::seconds(app_settings_.network.connect_timeout_seconds)) ==
        std::future_status::timeout) {
        socket.cancel();
        socket.close(socket_error_code);
        log::Error("Service", {"name", "Node Hub", "action", "connect", "remote", remote, "error", "timeout"});
        return;
    }
    if (socket_error_code) {
        log::Error("Service",
                   {"name", "Node Hub", "action", "connect", "remote", remote, "error", socket_error_code.message()});
        socket.close(socket_error_code);
        return;
    }

    set_common_socket_options(socket);
    ++total_connections_;
    ++current_active_outbound_connections_;

    const std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, connection, asio_context_, std::move(socket), tls_client_context_.get(),
            [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
                on_node_received_message(node, std::move(message));
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);

    {
        const std::lock_guard<std::mutex> lock(nodes_mutex_);
        nodes_.try_emplace(new_node->id(), new_node);
    }
    new_node->start();
}

void NodeHub::start_accept() {
    if (is_stopping()) return;
    if (log::test_verbosity(log::Level::kTrace)) [[unlikely]] {
        log::Trace("Service", {"name", "Node Hub", "status", "Listening for connections ..."});
    }
    socket_acceptor_.async_accept(
        [this](const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket) {
            handle_accept(error_code, std::move(socket));
        });
}

void NodeHub::handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket) {
    if (is_stopping() or error_code == boost::asio::error::operation_aborted) {
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

    ++total_connections_;
    // Check we do not exceed the maximum number of connections
    if (size() >= app_settings_.network.max_active_connections) {
        ++total_rejected_connections_;
        log::Warning("Service", {"name", "Node Hub", "action", "accept", "error", "max active connections reached"});
        close_socket(socket);
        start_accept();
        return;
    }

    // Check we do not exceed the maximum number of connections per IP
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

    set_common_socket_options(socket);
    const IPEndpoint remote{socket.remote_endpoint()};
    const IPEndpoint local{socket.local_endpoint()};
    const IPConnection connection(remote, IPConnectionType::kInbound);

    const std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, connection, asio_context_, std::move(socket), tls_server_context_.get(),
            [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
                on_node_received_message(node, std::move(message));
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);
    log::Info("Service", {"name", "Node Hub", "action", "accept", "local", local.to_string(), "remote",
                          remote.to_string(), "id", std::to_string(new_node->id())});

    ++current_active_inbound_connections_;
    {
        const std::lock_guard<std::mutex> lock(nodes_mutex_);
        nodes_.try_emplace(new_node->id(), new_node);
        connected_addresses_[*new_node->remote_endpoint().address_]++;
    }

    new_node->start();
    asio::post(asio_strand_, [this]() { start_accept(); });  // Continue listening for new connections
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

void NodeHub::on_node_disconnected(const std::shared_ptr<Node> node) {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);

    if (auto item{connected_addresses_.find(*node->remote_endpoint().address_)};
        item not_eq connected_addresses_.end()) {
        if (--item->second == 0) {
            connected_addresses_.erase(item);
        }
    }

    if (auto item{nodes_.find(node->id())}; item not_eq nodes_.end()) {
        item->second.reset();  // Release the ownership of the shared_ptr (other copies may exist in flight)
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
    }

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
    if (is_stopping() or node->is_stopping()) return;

    std::string error{};

    // This function behaves as a collector of messages from nodes
    if (message->get_type() == abi::NetMessageType::kAddr) {
        abi::MsgAddrPayload addr_payload{};
        const auto ret{addr_payload.deserialize(message->data())};
        if (not ret) {
            // TODO Pass it to the address manager
            for (const auto& service : addr_payload.identifiers_) {
                if (app_settings_.network.ipv4_only and service.endpoint_.address_.get_type() == IPAddressType::kIPv6)
                    continue;
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
    if (nodes_.contains(node_id)) {
        return nodes_.at(node_id);
    }
    return nullptr;
}

bool NodeHub::contains(int node_id) const {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);
    return nodes_.contains(node_id);
}

size_t NodeHub::size() const {
    const std::lock_guard<std::mutex> lock(nodes_mutex_);
    return nodes_.size();
}

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
