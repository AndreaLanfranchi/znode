/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <chrono>

#include <boost/asio/any_io_executor.hpp>

#include <core/common/outcome.hpp>

namespace znode::net {

//! \brief Check if system time is synchronized with a time server
//! \param executor [in] The executor to use for async operations
//! \param time_server [in] The time server to use for the check (time.nist.gov is used if empty)
//! \param max_skew_seconds [in] The maximum allowed skew in seconds (0 means no check)
outcome::result<void> check_system_time(boost::asio::any_io_executor executor, const std::string& time_server,
                                        uint32_t max_skew_seconds = 0);

}  // namespace znode::net
