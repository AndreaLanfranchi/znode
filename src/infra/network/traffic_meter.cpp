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

#include "traffic_meter.hpp"

namespace znode::net {

void TrafficMeter::update_inbound(size_t bytes) noexcept {
    const std::lock_guard lock{mutex_};
    cumulative_inbound_bytes_ += bytes;
    interval_inbound_bytes_ += bytes;
}

void TrafficMeter::update_outbound(size_t bytes) noexcept {
    const std::lock_guard lock{mutex_};
    cumulative_outbound_bytes_ += bytes;
    interval_outbound_bytes_ += bytes;
}

std::pair<size_t, size_t> TrafficMeter::get_cumulative_bytes() const noexcept {
    const std::lock_guard lock{mutex_};
    return {cumulative_inbound_bytes_, cumulative_outbound_bytes_};
}

std::pair<size_t, size_t> TrafficMeter::get_cumulative_speed() const noexcept {
    const std::lock_guard lock{mutex_};
    const auto elapsed_time{std::chrono::steady_clock::now() - start_time_};
    const auto elapsed_seconds{
        static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count())};
    return {elapsed_seconds ? cumulative_inbound_bytes_ / elapsed_seconds : cumulative_inbound_bytes_,
            elapsed_seconds ? cumulative_outbound_bytes_ / elapsed_seconds : cumulative_outbound_bytes_};
}

std::pair<size_t, size_t> TrafficMeter::get_interval_bytes(bool reset_interval) noexcept {
    const std::lock_guard lock{mutex_};
    std::pair<size_t, size_t> ret{interval_inbound_bytes_, interval_outbound_bytes_};
    if (reset_interval) {
        interval_inbound_bytes_ = 0;
        interval_outbound_bytes_ = 0;
        interval_time_ = std::chrono::steady_clock::now();
    }
    return ret;
}

std::pair<size_t, size_t> TrafficMeter::get_interval_speed(bool reset_interval) noexcept {
    const std::lock_guard lock{mutex_};
    const auto elapsed_time{std::chrono::steady_clock::now() - interval_time_};
    const auto elapsed_seconds{
        static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count())};
    std::pair<size_t, size_t> speed{
        elapsed_seconds ? interval_inbound_bytes_ / elapsed_seconds : interval_inbound_bytes_,
        elapsed_seconds ? interval_outbound_bytes_ / elapsed_seconds : interval_outbound_bytes_};
    if (reset_interval) [[likely]] {
        interval_time_ = std::chrono::steady_clock::now();
        interval_inbound_bytes_ = 0;
        interval_outbound_bytes_ = 0;
    }
    return speed;
}

void TrafficMeter::reset() noexcept {
    const std::lock_guard lock{mutex_};
    cumulative_inbound_bytes_ = 0;
    cumulative_outbound_bytes_ = 0;
    interval_inbound_bytes_ = 0;
    interval_outbound_bytes_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    interval_time_ = start_time_;
}
}  // namespace znode::net
