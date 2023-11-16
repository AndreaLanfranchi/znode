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

#include "time.hpp"

#include <format>

namespace znode {
using namespace std::chrono_literals;
std::chrono::time_point<NodeClock> NodeClock::now() noexcept {
    const auto ret{SteadyClock::now().time_since_epoch()};
    return std::chrono::time_point<NodeClock>{std::chrono::duration_cast<NodeClock::duration>(ret)};
}

std::string format_ISO8601(int64_t unixseconds, bool include_time) noexcept {
    std::tm time_storage{};
    const auto time_val{static_cast<std::time_t>(unixseconds)};
#if defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64)
    if (gmtime_s(&time_storage, &time_val) != 0) return {};
#else
    if (gmtime_r(&time_val, &time_storage) == nullptr) return {};
#endif
    time_storage.tm_year += 1900;
    time_storage.tm_mon += 1;
    std::string ret{
        std::format("{:04d}-{:02d}-{:02d}", time_storage.tm_year, time_storage.tm_mon, time_storage.tm_mday)};
    if (include_time) {
        ret += std::format("T{:02d}:{:02d}:{:02d}Z", time_storage.tm_hour, time_storage.tm_min, time_storage.tm_sec);
    }
    return ret;
}
std::string format_ISO8601(const Seconds& time_point, bool include_time) noexcept {
    return format_ISO8601(time_point.time_since_epoch().count(), include_time);
}
}  // namespace znode
