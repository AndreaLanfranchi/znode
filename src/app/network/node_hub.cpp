/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/lexical_cast.hpp>

#include <core/common/misc.hpp>

#include <app/common/log.hpp>
#include <app/network/common.hpp>
#include <app/network/node_hub.hpp>

namespace zenpp::network {

using namespace boost;
using asio::ip::tcp;

bool NodeHub::start() {
    if (bool expected{false}; !is_started_.compare_exchange_strong(expected, true)) {
        return false;
    }

    if (app_settings_.network.use_tls) {
        const auto ssl_data{(*app_settings_.data_directory)[DataDirectory::kSSLCert].path()};
        auto ctx{generate_tls_context(TLSContextType::kServer, ssl_data, app_settings_.network.tls_password)};
        if (!ctx) {
            log::Error("NodeHub", {"action", "start", "error", "failed to generate TLS server context"});
            return false;
        }
        tls_server_context_ = std::make_unique<asio::ssl::context>(ctx);
        ctx = generate_tls_context(TLSContextType::kClient, ssl_data, app_settings_.network.tls_password);
        if (!ctx) {
            log::Error("NodeHub", {"action", "start", "error", "failed to generate TLS server context"});
            return false;
        }
        tls_client_context_ = std::make_unique<asio::ssl::context>(ctx);
    }

    initialize_acceptor();
    info_stopwatch_.start(true);
    start_service_timer();
    start_accept();
    asio::post(asio_strand_, [this]() { start_connecting(); });
    return true;
}

void NodeHub::start_connecting() {
    // Connect nodes if required
    if (!app_settings_.network.connect_nodes.empty()) {
        for (auto const& node_address : app_settings_.network.connect_nodes) {
            if (is_stopping()) return;
            const NetworkAddress address{node_address};
            std::ignore = connect(address, NodeConnectionMode::kManualOutbound);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (current_active_connections_.load()) {
        // TODO: Or we should continue if force dns seeding is enabled?
        return;
    }

    // TODO: Seeding nodes
    // TODO: Get them from chain configuration parameters
    const std::vector<std::string> seeds{"dnsseed.horizen.global", "dnsseed.zensystem.io", "mainnet.horizen.global",
                                         "mainnet.zensystem.io", "node1.zenchain.info"};

    boost::asio::ip::tcp::resolver resolver{asio_context_};
    for (const auto& host : seeds) {
        if (is_stopping()) return;
        // Syncronous resolve
        boost::system::error_code ec;
        const auto results{resolver.resolve(host, "", ec)};
        if (ec) {
            log::Error("NodeHub", {"action", "start_connecting", "error", ec.message()});
            continue;
        }
        for (const auto& result : results) {
            if (is_stopping()) return;
            if (!result.endpoint().address().is_v4()) {
                continue;
            }
            NetworkAddress address{result.endpoint().address(), 9033 /*TODO: Get from chain params*/};
            std::ignore = connect(address);
        }
    }
}

void NodeHub::start_service_timer() {
    service_timer_.expires_after(std::chrono::seconds(kServiceTimerIntervalSeconds_));
    service_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (handle_service_timer(ec)) {
            start_service_timer();
        }
    });
}

bool NodeHub::handle_service_timer(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) return false;
    if (ec) {
        log::Error("Service", {"name", "Node Hub", "action", "info_timer_tick", "error", ec.message()});
        return false;
    }
    print_info();  // Print info every 5 seconds
    std::scoped_lock lock{nodes_mutex_};
    for (auto [node_id, node_ptr] : nodes_) {
        if (const auto result{node_ptr->is_idle()}; result != NodeIdleResult::kNotIdle) {
            std::string reason{magic_enum::enum_name(result)};
            log::Info("Service", {"name", "Node Hub", "action", "service", "node", std::to_string(node_id), "remote",
                                  node_ptr->to_string(), "reason", reason})
                << "Disconnecting ...";
            node_ptr->stop(false);
        }
    }
    return !is_stopping();  // Required to resubmit the timer
}

void NodeHub::print_info() {
    // Let each cycle to last at least 5 seconds to have meaningful data and, of course, to avoid division by zero
    const auto info_lap_duration{
        static_cast<uint32_t>(duration_cast<std::chrono::seconds>(info_stopwatch_.since_start()).count())};
    if (info_lap_duration < 5) {
        return;
    }

    auto current_total_bytes_received{total_bytes_received_.load()};
    auto current_total_bytes_sent{total_bytes_sent_.load()};
    auto period_total_bytes_received{current_total_bytes_received - last_info_total_bytes_received_.load()};
    auto period_total_bytes_sent{current_total_bytes_sent - last_info_total_bytes_sent_.load()};

    std::vector<std::string> info_data;
    info_data.insert(info_data.end(), {"peers i/o", std::to_string(current_active_inbound_connections_.load()) + "/" +
                                                        std::to_string(current_active_outbound_connections_.load())});
    info_data.insert(info_data.end(), {"data i/o", to_human_bytes(current_total_bytes_received, true) + " " +
                                                       to_human_bytes(current_total_bytes_sent, true)});

    auto period_bytes_received_per_second{to_human_bytes(period_total_bytes_received / info_lap_duration, true) + "s"};
    auto period_bytes_sent_per_second{to_human_bytes(period_total_bytes_sent / info_lap_duration, true) + "s"};

    info_data.insert(info_data.end(),
                     {"speed i/o", period_bytes_received_per_second + " " + period_bytes_sent_per_second});

    last_info_total_bytes_received_.store(current_total_bytes_received);
    last_info_total_bytes_sent_.store(current_total_bytes_sent);

    log::Info("Network usage", info_data);
    info_stopwatch_.start(true);
}

bool NodeHub::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopping */ {
        service_timer_.cancel();
        socket_acceptor_.close();
        std::unique_lock lock(nodes_mutex_);
        // Stop all nodes
        for (auto [node_id, node_ptr] : nodes_) {
            node_ptr->stop(false);
        }
        lock.unlock();
        // Wait for all nodes to stop - active_connections get to zero
        while (wait && current_active_connections_.load() > 0) {
            log::Info("Service", {"name", "Node Hub", "action", "stop", "info", "waiting for active connections"})
                << std::to_string(current_active_connections_.load());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return ret;
}

bool NodeHub::connect(const NetworkAddress& address, const NodeConnectionMode mode) {
    if (is_stopping()) return false;

    const std::string remote{network::to_string(address.to_endpoint())};

    log::Info("Service", {"name", "Node Hub", "action", "connect", "remote", remote});
    if (current_active_connections_ >= app_settings_.network.max_active_connections) {
        log::Error("Service", {"name", "Node Hub", "action", "connect", "error", "max active connections reached"});
        return false;
    } else {
        std::scoped_lock lock{nodes_mutex_};
        if (auto item = connected_addresses_.find(address.address); item != connected_addresses_.end()) {
            if (item->second >= app_settings_.network.max_active_connections_per_ip) {
                log::Error("Service",
                           {"name", "Node Hub", "action", "connect", "error", "max active connections per ip reached"});
                return false;
            }
        }
    };

    // Create the socket and try connect
    boost::asio::ip::tcp::socket socket{asio_context_};
    try {
        socket.connect(address.to_endpoint());
        set_common_socket_options(socket);
    } catch (const boost::system::system_error& ex) {
        log::Error("Service", {"name", "Node Hub", "action", "connect", "remote", remote, "error", ex.what()});
        return false;
    }

    ++total_connections_;
    ++current_active_connections_;
    ++current_active_outbound_connections_;

    std::shared_ptr<Node> new_node(new Node(
                                       app_settings_, mode, asio_context_, std::move(socket), tls_client_context_.get(),
                                       [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
                                       [this](DataDirectionMode direction, size_t bytes_transferred) {
                                           on_node_data(direction, bytes_transferred);
                                       }),
                                   Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);

    {
        std::scoped_lock lock(nodes_mutex_);
        nodes_.emplace(new_node->id(), new_node);
    }
    new_node->start();
    return true;
}

void NodeHub::start_accept() {
    if (is_stopping()) return;

    log::Trace("Service", {"name", "Node Hub", "status", "Waiting for connections ..."});
    socket_acceptor_.async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
        handle_accept(ec, std::move(socket));
    });
}

void NodeHub::handle_accept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
    if (is_stopping() || ec == boost::asio::error::operation_aborted) return;

    boost::system::error_code local_ec;  // Just to ignore it
    const auto close_socket{[&local_ec](boost::asio::ip::tcp::socket& socket) {
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, local_ec);
        socket.close();
    }};

    if (ec) {
        log::Error("Service", {"name", "Node Hub", "action", "accept", "error", ec.message()});
        close_socket(socket);
        start_accept();
        return;
    }

    socket.non_blocking(true, local_ec);
    if (local_ec) {
        log::Error("Service", {"name", "Node Hub", "action", "handle", "error", local_ec.message()});
        close_socket(socket);
        start_accept();
        return;
    }

    ++total_connections_;
    // Check we do not exceed the maximum number of connections
    if (current_active_connections_ >= app_settings_.network.max_active_connections) {
        ++total_rejected_connections_;
        log::Warning("Service", {"name", "Node Hub", "action", "accept", "error", "max active connections reached"});
        close_socket(socket);
        start_accept();
        return;
    }

    // Check we do not exceed the maximum number of connections per IP
    if (auto item = connected_addresses_.find(socket.remote_endpoint().address()); item != connected_addresses_.end()) {
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
    std::string remote{network::to_string(socket.remote_endpoint())};
    std::string local{network::to_string(socket.local_endpoint())};
    std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, NodeConnectionMode::kInbound, asio_context_, std::move(socket), tls_server_context_.get(),
            [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);
    log::Info("Service", {"name", "Node Hub", "action", "accept", "local", local, "remote", remote, "id",
                          std::to_string(new_node->id())});

    ++current_active_connections_;
    ++current_active_inbound_connections_;
    {
        std::scoped_lock lock(nodes_mutex_);
        nodes_.emplace(new_node->id(), new_node);
        connected_addresses_[new_node->remote_endpoint().address()]++;
    }

    new_node->start();
    asio::post(asio_strand_, [this]() { start_accept(); });  // Continue listening for new connections
}

void NodeHub::initialize_acceptor() {
    std::vector<std::string> address_parts;
    boost::split(address_parts, app_settings_.network.local_endpoint, boost::is_any_of(":"));
    if (address_parts.size() != 2) {
        throw std::invalid_argument("Invalid local endpoint: " + app_settings_.network.local_endpoint);
    }

    auto port{boost::lexical_cast<uint16_t>(address_parts[1])};
    const auto local_endpoint{tcp::endpoint(boost::asio::ip::make_address(address_parts[0]), port)};

    socket_acceptor_.open(tcp::v4());
    socket_acceptor_.set_option(tcp::acceptor::reuse_address(true));
    socket_acceptor_.set_option(tcp::no_delay(true));
    socket_acceptor_.set_option(boost::asio::socket_base::keep_alive(true));
    socket_acceptor_.set_option(boost::asio::socket_base::receive_buffer_size(static_cast<int>(64_KiB)));
    socket_acceptor_.set_option(boost::asio::socket_base::send_buffer_size(static_cast<int>(64_KiB)));
    socket_acceptor_.bind(local_endpoint);
    socket_acceptor_.listen();

    log::Info("Service", {"name", "Node Hub", "secure", (app_settings_.network.use_tls ? "yes" : "no"), "listening on",
                          to_string(local_endpoint)});
}

void NodeHub::on_node_disconnected(const std::shared_ptr<Node>& node) {
    std::scoped_lock lock(nodes_mutex_);

    if (auto item{connected_addresses_.find(node->remote_endpoint().address())}; item != connected_addresses_.end()) {
        if (--item->second == 0) {
            connected_addresses_.erase(item);
        }
    }

    if (nodes_.contains(node->id())) {
        nodes_.erase(node->id());
        ++total_disconnections_;
        current_active_connections_.store(static_cast<uint32_t>(nodes_.size()));
        switch (node->mode()) {
            using enum NodeConnectionMode;
            case kInbound:
                if (current_active_inbound_connections_) --current_active_inbound_connections_;
                break;
            case kOutbound:
            case kManualOutbound:
                if (current_active_outbound_connections_) --current_active_outbound_connections_;
                break;
            default:
                ASSERT(false);  // Should never happen
        }
    }

    if (app_settings_.log.log_verbosity == log::Level::kTrace) {
        log::Trace("Service", {"name", "Node Hub", "total connections", std::to_string(total_connections_),
                               "total disconnections", std::to_string(total_disconnections_),
                               "total rejected connections", std::to_string(total_rejected_connections_)});
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
    }
}

std::shared_ptr<Node> NodeHub::operator[](int id) const {
    std::scoped_lock lock(nodes_mutex_);
    if (nodes_.contains(id)) {
        return nodes_.at(id);
    }
    return nullptr;
}

bool NodeHub::contains(int id) const {
    std::scoped_lock lock(nodes_mutex_);
    return nodes_.contains(id);
}

size_t NodeHub::size() const {
    std::scoped_lock lock(nodes_mutex_);
    return nodes_.size();
}

std::vector<std::shared_ptr<Node>> NodeHub::get_nodes() const {
    std::scoped_lock lock(nodes_mutex_);
    std::vector<std::shared_ptr<Node>> nodes;
    for (const auto& [id, node] : nodes_) {
        nodes.emplace_back(node);
    }
    return nodes;
}

void NodeHub::set_common_socket_options(tcp::socket& socket) {
    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));

    socket.set_option(tcp::no_delay(true));
    socket.set_option(tcp::socket::keep_alive(true));
    socket.set_option(boost::asio::socket_base::linger(true, 5));
    socket.set_option(boost::asio::socket_base::receive_buffer_size(static_cast<int>(64_KiB)));
    socket.set_option(boost::asio::socket_base::send_buffer_size(static_cast<int>(64_KiB)));
}
}  // namespace zenpp::network
