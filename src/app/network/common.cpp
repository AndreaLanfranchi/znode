/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/network/common.hpp>

namespace zenpp::network {

std::optional<boost::asio::ip::tcp::endpoint> get_remote_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept {
    boost::system::error_code error_code;
    auto endpoint = socket.remote_endpoint(error_code);
    if (error_code) {
        return std::nullopt;
    }
    return endpoint;
}

std::optional<boost::asio::ip::tcp::endpoint> get_local_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept {
    boost::system::error_code error_code;
    auto endpoint = socket.local_endpoint(error_code);
    if (error_code) {
        return std::nullopt;
    }
    return endpoint;
}

std::string to_string(const boost::asio::ip::tcp::endpoint& endpoint) {
    std::string ret;
    const auto tmp_address = endpoint.address();
    if (tmp_address.is_v6()) ret += "[";
    ret += tmp_address.to_string();
    if (tmp_address.is_v6()) ret += "]";
    ret += ":" + std::to_string(endpoint.port());
    return ret;
}
}  // namespace zenpp::network
