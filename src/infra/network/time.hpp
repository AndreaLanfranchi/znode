/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <chrono>

#include <boost/asio/any_io_executor.hpp>

#include <core/common/outcome.hpp>

namespace zenpp::net {

//! \brief Check if system time is synchronized with a time server
outcome::result<void> check_system_time(boost::asio::any_io_executor executor, const std::string& time_server,
                                        uint32_t max_skew_seconds = 0);

}  // namespace zenpp::net