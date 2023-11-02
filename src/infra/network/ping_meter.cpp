/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "ping_meter.hpp"

#include <core/common/assert.hpp>

namespace znode::net {

using namespace std::chrono;
using namespace std::chrono_literals;

PingMeter::PingMeter(float alpha) : alpha_{alpha} { ASSERT(alpha > 0.0F and alpha < 1.0F); }

void PingMeter::start_sample() noexcept {
    const std::lock_guard lock{mutex_};
    if (ping_in_progress_) return;
    ping_start_ = steady_clock::now();
    ping_in_progress_ = true;
}

void PingMeter::end_sample() noexcept {
    const auto time_now{std::chrono::steady_clock::now()};
    const std::lock_guard lock{mutex_};
    if (not ping_in_progress_) return;
    ping_in_progress_ = false;

    ASSERT(time_now >= ping_start_);
    const auto ping_duration_ms = duration_cast<milliseconds>(std::chrono::steady_clock::now() - ping_start_);

    ping_start_ = std::chrono::steady_clock::time_point::min();
    ping_nonce_.reset();
    if (ping_duration_ms < 0ms) return;  // Irrelevant sample

    if (ping_duration_ms_ema_ == 0ms) {
        ping_duration_ms_ema_ = ping_duration_ms;
        ping_duration_ms_min_ = ping_duration_ms;
        ping_duration_ms_max_ = ping_duration_ms;
    } else {
        ping_duration_ms_ema_ = static_cast<milliseconds>(
            static_cast<long>(alpha_ * static_cast<float>(ping_duration_ms.count()) +
                              (1.0F - alpha_) * static_cast<float>(ping_duration_ms_ema_.count())));
        if (ping_duration_ms < ping_duration_ms_min_) ping_duration_ms_min_ = ping_duration_ms;
        if (ping_duration_ms > ping_duration_ms_max_) ping_duration_ms_max_ = ping_duration_ms;
    }
}

void PingMeter::set_nonce(uint64_t nonce) noexcept {
    const std::lock_guard lock{mutex_};
    ping_nonce_.emplace(nonce);
}

std::optional<uint64_t> PingMeter::get_nonce() const noexcept {
    const std::lock_guard lock{mutex_};
    return ping_nonce_;
}

bool PingMeter::pending_sample() const noexcept {
    const std::lock_guard lock{mutex_};
    return ping_in_progress_;
}

std::chrono::milliseconds PingMeter::pending_sample_duration() const noexcept {
    const std::lock_guard lock{mutex_};
    if (!ping_in_progress_) return 0ms;
    ASSERT(std::chrono::steady_clock::now() >= ping_start_);
    return duration_cast<milliseconds>(std::chrono::steady_clock::now() - ping_start_);
}

std::chrono::milliseconds PingMeter::get_ema() const noexcept {
    const std::lock_guard lock{mutex_};
    return ping_duration_ms_ema_;
}

std::chrono::milliseconds PingMeter::get_min() const noexcept {
    const std::lock_guard lock{mutex_};
    return ping_duration_ms_min_;
}

std::chrono::milliseconds PingMeter::get_max() const noexcept {
    const std::lock_guard lock{mutex_};
    return ping_duration_ms_max_;
}
}  // namespace znode::net
