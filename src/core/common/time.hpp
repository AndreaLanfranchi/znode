/*
   Copyright (c) 2009-2010 Satoshi Nakamoto
   Copyright (c) 2009-2022 The Bitcoin Core developers
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
#include <cstdint>
#include <string>

namespace znode {

using SteadyClock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;
using Seconds = std::chrono::time_point<SteadyClock, std::chrono::seconds>;
using Milliseconds = std::chrono::time_point<SteadyClock, std::chrono::milliseconds>;
using Microseconds = std::chrono::time_point<SteadyClock, std::chrono::microseconds>;

struct NodeClock : public SystemClock {
    using time_point = std::chrono::time_point<NodeClock>;
    using duration = time_point::duration;
    static time_point now() noexcept;
};

using NodeSeconds = std::chrono::time_point<NodeClock, std::chrono::seconds>;

//! \brief Returns a time point representing the current time
template <typename T>
T Now() noexcept {
    return std::chrono::time_point_cast<T::duration>(T::clock::now());
}

//! \brief Returns a string formatted according to ISO 8601 of a time point
std::string format_ISO8601(int64_t unixseconds, bool include_time = true) noexcept;

//! \brief Returns a string formatted according to ISO 8601 of a time point
std::string format_ISO8601(const SteadyClock::time_point& time_point, bool include_time = true) noexcept;

}  // namespace znode
