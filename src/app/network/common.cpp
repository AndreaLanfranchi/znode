/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/network/common.hpp>

namespace zenpp::network {

std::optional<boost::asio::ip::tcp::endpoint> get_remote_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept {
    boost::system::error_code ec;
    auto endpoint = socket.remote_endpoint(ec);
    if (ec) {
        return std::nullopt;
    }
    return endpoint;
}

std::optional<boost::asio::ip::tcp::endpoint> get_local_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept {
    boost::system::error_code ec;
    auto endpoint = socket.local_endpoint(ec);
    if (ec) {
        return std::nullopt;
    }
    return endpoint;
}
std::string to_string(const boost::asio::ip::tcp::endpoint& endpoint) {
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}
}  // namespace zenpp::network
