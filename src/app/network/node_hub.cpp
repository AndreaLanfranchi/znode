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
                         [this](unsigned interval) -> unsigned { return on_service_timer_expired(interval); });
    start_accept();
    asio::post(asio_strand_, [this]() { start_connecting(); });
    return true;
}

bool NodeHub::stop(bool wait) noexcept {
    const auto ret{Stoppable::stop(wait)};
    if (ret) /* not already stopping */ {
        service_timer_.stop(false);
        socket_acceptor_.close();
        std::unique_lock lock(nodes_mutex_);
        // Stop all nodes
        for (auto [node_id, node_ptr] : nodes_) {
#if defined(__clang__) and __clang_major__ <= 15
            // Workaround for clang <=15 bug
            // cause structured bindings are allowed by C++20 to be captured in lambdas
            auto node_ptr_copy{node_ptr};
            asio::post(asio_strand_, [node_ptr_copy]() { std::ignore = node_ptr_copy->stop(false); });
#else
            asio::post(asio_strand_, [node_ptr]() { std::ignore = node_ptr->stop(false); });
#endif
        }
        lock.unlock();
        // Wait for all nodes to stop - active_connections get to zero
        while (wait and current_active_connections_.load() > 0) {
            log::Info("Service", {"name", "Node Hub", "action", "stop", "pending connections",
                                  std::to_string(current_active_connections_.load())});
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return ret;
}

void NodeHub::start_connecting() {
    // Connect nodes if required
    if (!app_settings_.network.connect_nodes.empty()) {
        for (auto const& str : app_settings_.network.connect_nodes) {
            if (is_stopping()) return;
            const IPEndpoint endpoint{str};
            std::ignore = connect(endpoint, NodeConnectionMode::kManualOutbound);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (current_active_connections_.load() not_eq 0U) {
        // TODO: Or we should continue if force dns seeding is enabled?
        return;
    }

    boost::asio::ip::tcp::resolver resolver{asio_context_};
    const auto seeds{get_chain_seeds(*app_settings_.chain_config)};

    for (const auto& host : seeds) {
        if (is_stopping()) return;
        // Syncronous resolve
        boost::system::error_code error_code;
        const auto results{resolver.resolve(host, "", error_code)};
        if (error_code) {
            log::Error("NodeHub", {"action", "start_connecting", "error", error_code.message()});
            continue;
        }
        for (const auto& result : results) {
            if (is_stopping()) return;
            if (not result.endpoint().address().is_v4()) continue;
            const IPEndpoint endpoint{result.endpoint().address(),
                                      gsl::narrow_cast<uint16_t>(app_settings_.chain_config->default_port_)};
            std::ignore = connect(endpoint);
        }
    }
}

unsigned NodeHub::on_service_timer_expired(unsigned interval) {
    print_info();  // Print info every 5 seconds
    const std::unique_lock lock{nodes_mutex_};
    for (auto /*copy !!*/ [node_id, node_ptr] : nodes_) {
        if (const auto result{node_ptr->is_idle()}; result not_eq NodeIdleResult::kNotIdle) {
            const std::string reason{magic_enum::enum_name(result)};
            log::Warning("Service", {"name", "Node Hub", "action", "handle_service_timer[idle_check]", "node",
                                     std::to_string(node_id), "remote", node_ptr->to_string(), "reason", reason})
                << "Disconnecting ...";

#if defined(__clang__) and __clang_major__ <= 15
            // Workaround for clang <=15 bug
            // cause structured bindings are allowed by C++20 to be captured in lambdas
            auto node_ptr_copy{node_ptr};
            asio::post(asio_strand_, [node_ptr_copy]() { std::ignore = node_ptr_copy->stop(false); });
#else
            asio::post(asio_strand_, [node_ptr]() { std::ignore = node_ptr->stop(false); });
#endif
        }
    }
    return is_stopping() ? 0U : interval;
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

bool NodeHub::connect(const IPEndpoint& endpoint, const NodeConnectionMode mode) {
    if (is_stopping()) return false;

    const std::string remote{endpoint.to_string()};

    log::Info("Service", {"name", "Node Hub", "action", "connect", "remote", remote});
    if (current_active_connections_ >= app_settings_.network.max_active_connections) {
        log::Error("Service", {"name", "Node Hub", "action", "connect", "error", "max active connections reached"});
        return false;
    }
    {
        const std::scoped_lock lock{nodes_mutex_};
        if (auto item = connected_addresses_.find(*endpoint.address_);
            item not_eq connected_addresses_.end() and
            item->second >= app_settings_.network.max_active_connections_per_ip) {
            log::Error("Service",
                       {"name", "Node Hub", "action", "connect", "error", "max active connections per ip reached"});
            return false;
        }
    }

    // Create the socket and try connect
    boost::asio::ip::tcp::socket socket{asio_context_};
    try {
        socket.connect(endpoint.to_endpoint());
        set_common_socket_options(socket);
    } catch (const boost::system::system_error& ex) {
        log::Error("Service", {"name", "Node Hub", "action", "connect", "remote", remote, "error", ex.what()});
        return false;
    }

    ++total_connections_;
    ++current_active_connections_;
    ++current_active_outbound_connections_;

    const std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, mode, asio_context_, std::move(socket), tls_client_context_.get(),
            [this](const std::shared_ptr<Node>& node) { on_node_disconnected(node); },
            [this](DataDirectionMode direction, size_t bytes_transferred) {
                on_node_data(direction, bytes_transferred);
            },
            [this](std::shared_ptr<Node> node, std::shared_ptr<abi::NetMessage> message) {
                on_node_received_message(node, std::move(message));
            }),
        Node::clean_up /* ensures proper shutdown when shared_ptr falls out of scope*/);

    {
        const std::scoped_lock lock(nodes_mutex_);
        nodes_.emplace(new_node->id(), new_node);
    }
    new_node->start();
    return true;
}

void NodeHub::start_accept() {
    if (is_stopping()) return;
    if (app_settings_.log.log_verbosity == log::Level::kTrace) [[unlikely]] {
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
    if (current_active_connections_ >= app_settings_.network.max_active_connections) {
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
    const std::shared_ptr<Node> new_node(
        new Node(
            app_settings_, NodeConnectionMode::kInbound, asio_context_, std::move(socket), tls_server_context_.get(),
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

    ++current_active_connections_;
    ++current_active_inbound_connections_;
    {
        const std::scoped_lock lock(nodes_mutex_);
        nodes_.emplace(new_node->id(), new_node);
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

void NodeHub::on_node_disconnected(const std::shared_ptr<Node>& node) {
    const std::unique_lock lock(nodes_mutex_);

    if (auto item{connected_addresses_.find(*node->remote_endpoint().address_)};
        item not_eq connected_addresses_.end()) {
        if (--item->second == 0) {
            connected_addresses_.erase(item);
        }
    }

    if (nodes_.contains(node->id())) {
        nodes_.erase(node->id());
        ++total_disconnections_;
        current_active_connections_.store(gsl::narrow_cast<uint32_t>(nodes_.size()));
        switch (node->mode()) {
            using enum NodeConnectionMode;
            case kInbound:
                if (current_active_inbound_connections_ not_eq 0U) {
                    --current_active_inbound_connections_;
                }
                break;
            case kOutbound:
            case kManualOutbound:
                if (current_active_outbound_connections_ not_eq 0U) {
                    --current_active_outbound_connections_;
                }
                break;
            default:
                ASSERT(false);  // Should never happen
        }
    }

    if (app_settings_.log.log_verbosity == log::Level::kTrace) [[unlikely]] {
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

void NodeHub::on_node_received_message(std::shared_ptr<Node>& node, std::shared_ptr<abi::NetMessage> message) {
    using namespace serialization;
    using enum Error;

    std::string error{};

    REQUIRES(message not_eq nullptr);

    // This function behaves as a collector of messages from nodes
    if (message->get_type() == abi::NetMessageType::kAddr) {
        abi::MsgAddrPayload addr_payload{};
        const auto ret{addr_payload.deserialize(message->data())};
        if (not ret) {
            // TODO Pass it to the address manager
        } else {
            error = absl::StrCat("error ", magic_enum::enum_name(ret));
        }
    }

    if (not error.empty() or app_settings_.log.log_verbosity >= log::Level::kTrace) [[unlikely]] {
        const std::vector<std::string> log_params{
            "name",    "Node Hub",
            "action",  __func__,
            "command", std::string{magic_enum::enum_name(message->header().get_type())},
            "remote",  node->to_string(),
            "status",  error.empty() ? "success" : error};
        log::BufferBase((error.empty() ? log::Level::kTrace : log::Level::kError), "Service", log_params)
            << (error.empty() ? "" : "Disconnecting ...");
        if (not error.empty()) node->stop(false);
    }
}

std::shared_ptr<Node> NodeHub::operator[](int node_id) const {
    const std::unique_lock lock(nodes_mutex_);
    if (nodes_.contains(node_id)) {
        return nodes_.at(node_id);
    }
    return nullptr;
}

bool NodeHub::contains(int node_id) const {
    const std::unique_lock lock(nodes_mutex_);
    return nodes_.contains(node_id);
}

size_t NodeHub::size() const {
    const std::unique_lock lock(nodes_mutex_);
    return nodes_.size();
}

std::vector<std::shared_ptr<Node>> NodeHub::get_nodes() const {
    const std::unique_lock lock(nodes_mutex_);
    std::vector<std::shared_ptr<Node>> nodes;
    for (const auto& [id, node] : nodes_) {
        nodes.emplace_back(node);
    }
    return nodes;
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
