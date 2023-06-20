/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>

#include <boost/asio/ip/tcp.hpp>

namespace zen::network {

std::optional<boost::asio::ip::tcp::endpoint> get_remote_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept;

std::optional<boost::asio::ip::tcp::endpoint> get_local_endpoint(const boost::asio::ip::tcp::socket& socket) noexcept;

std::string to_string(const boost::asio::ip::tcp::endpoint& endpoint);

} // namespace zen::network