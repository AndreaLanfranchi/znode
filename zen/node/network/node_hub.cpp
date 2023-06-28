/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "node_hub.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <zen/core/common/misc.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/network/common.hpp>

namespace zen::network {

using boost::asio::ip::tcp;

void NodeHub::start() {
    initialize_acceptor();
    info_stopwatch_.start(true);
    start_service_timer();
    start_accept();
}

void NodeHub::start_service_timer() {
    service_timer_.expires_after(std::chrono::seconds(kServiceTimerIntervalSeconds_));
    service_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;  // No other operation allowed
        } else if (ec) {
            log::Error("Service", {"name", "Node Hub", "action", "info_timer_tick", "error", ec.message()});
            // Fall through and resubmit
        } else if (!ec) {
            print_info();
        }
        start_service_timer();
    });
}

void NodeHub::print_info() {
    // In the unlucky case this fires with subsecond duration (i.e. returns 0), we'll just skip this lap
    // to avoid division by zero
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
        std::scoped_lock lock(nodes_mutex_);
        // Stop all nodes
        for (auto& node : nodes_) {
            node->stop(false);
        }
    }
    return ret;
}

void NodeHub::start_accept() {
    log::Trace("Service", {"name", "Node Hub", "status", "Listening", "secure", ssl_server_context_ ? "yes" : "no"});

    std::shared_ptr<Node> new_node(new Node(
                                       NodeConnectionMode::kInbound, *node_settings_.asio_context, ssl_server_context_,
                                       node_settings_.network.idle_timeout_seconds,
                                       [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
                                       [this](DataDirectionMode direction, size_t bytes_transferred) {
                                           on_node_data(direction, bytes_transferred);
                                       }),
                                   Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);

    socket_acceptor_.async_accept(
        new_node->socket(), [this, new_node](const boost::system::error_code& ec) { handle_accept(new_node, ec); });
}

void NodeHub::handle_accept(const std::shared_ptr<Node>& new_node, const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        log::Trace("Service", {"name", "Node Hub", "status", "stop"});
        return;  // No other operation allowed
    }

    std::string remote{"unknown"};
    std::string local{"unknown"};
    if (auto remote_endpoint{get_remote_endpoint(new_node->socket())}; remote_endpoint) {
        remote = to_string(remote_endpoint.value());
    }
    if (auto local_endpoint{get_local_endpoint(new_node->socket())}; local_endpoint) {
        local = to_string(local_endpoint.value());
    }

    log::Trace("Service", {"name", "Node Hub", "status", "handle_accept", "local", local, "remote", remote});

    if (ec) {
        log::Error("Service", {"name", "Node Hub", "error", ec.message()});
        // Fall through and continue listening for new connections
    } else if (!ec) {
        ++total_connections_;
        // Check we do not exceed the maximum number of connections
        if (current_active_connections_ >= node_settings_.network.max_active_connections) {
            log::Trace("Service", {"name", "Node Hub", "peers",
                                   std::to_string(node_settings_.network.max_active_connections), "action", "reject"});
            new_node->stop(false);
            ++total_rejected_connections_;
            return;
        }

        ++current_active_connections_;
        ++current_active_inbound_connections_;

        log::Trace("Service", {"name", "Node Hub", "total connections", std::to_string(total_connections_),
                               "total disconnections", std::to_string(total_disconnections_),
                               "total rejected connections", std::to_string(total_rejected_connections_)});

        new_node->socket().set_option(tcp::no_delay(true));
        new_node->socket().set_option(tcp::socket::keep_alive(true));
        new_node->socket().set_option(boost::asio::socket_base::linger(true, 5));
        new_node->socket().set_option(boost::asio::socket_base::receive_buffer_size(static_cast<int>(64_KiB)));
        new_node->socket().set_option(boost::asio::socket_base::send_buffer_size(static_cast<int>(64_KiB)));
        new_node->start();

        std::scoped_lock lock(nodes_mutex_);
        nodes_.insert(new_node);
        // Fall through and continue listening for new connections
    }

    io_strand_.post([this]() { start_accept(); });  // Continue listening for new connections
}

void NodeHub::initialize_acceptor() {
    std::vector<std::string> address_parts;
    boost::split(address_parts, node_settings_.network.local_endpoint, boost::is_any_of(":"));
    if (address_parts.size() != 2) {
        throw std::runtime_error("Invalid local endpoint: " + node_settings_.network.local_endpoint);
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

    log::Info("Service", {"name", "Node Hub", "secure", (node_settings_.network.use_tls ? "yes" : "no"), "listening on",
                          to_string(local_endpoint)});
}

void NodeHub::on_node_disconnected(const std::shared_ptr<Node>& node) {
    if (current_active_connections_) --current_active_connections_;

    switch (node->mode()) {
        using enum NodeConnectionMode;
        case kInbound:
            if (current_active_inbound_connections_) --current_active_inbound_connections_;
            break;
        case kOutbound:
        case kManualOutbound:
            if (current_active_outbound_connections_) --current_active_outbound_connections_;
            break;
    }
    ++total_disconnections_;

    log::Trace("Service", {"name", "Node Hub", "total connections", std::to_string(total_connections_),
                           "total disconnections", std::to_string(total_disconnections_), "total rejected connections",
                           std::to_string(total_rejected_connections_)});

    std::scoped_lock lock(nodes_mutex_);
    nodes_.erase(node);
}

void NodeHub::on_node_data(zen::network::DataDirectionMode direction, const size_t bytes_transferred) {
    switch (direction) {
        case DataDirectionMode::kInbound:
            total_bytes_received_ += bytes_transferred;
            break;
        case DataDirectionMode::kOutbound:
            total_bytes_sent_ += bytes_transferred;
            break;
    }
}
}  // namespace zen::network
