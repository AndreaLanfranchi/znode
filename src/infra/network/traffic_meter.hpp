/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <utility>

namespace znode::net {

//! \brief A simple network traffic meter class.
//! \remarks This class is thread-safe.
class TrafficMeter {
  public:
    TrafficMeter() = default;

    //! \brief Updates the inbound traffic.
    void update_inbound(size_t bytes) noexcept;

    //! \brief Updates the outbound traffic.
    void update_outbound(size_t bytes) noexcept;

    //! \brief Returns the cumulative inbound and outbound traffic (in bytes).
    std::pair<size_t, size_t> get_cumulative_bytes() const noexcept;

    //! \brief Returns the cumulative inbound and outbound traffic speed (in bytes per second).
    std::pair<size_t, size_t> get_cumulative_speed() const noexcept;

    //! \brief Returns the inbound and outbound traffic (in bytes) during the last interval
    //! \param reset_interval [in] If true, the interval counters are reset.
    //! \remarks By default reset_interval is false.
    std::pair<size_t, size_t> get_interval_bytes(bool reset_interval = false) noexcept;

    //! \brief Returns the inbound and outbound traffic speed (in bytes per second) during the last interval
    //! \param reset_interval [in] If true, the interval counters are reset.
    //! \remarks By default reset_interval is true.
    std::pair<size_t, size_t> get_interval_speed(bool reset_interval = true) noexcept;

    //! \brief Resets the meter object to its factory state.
    void reset() noexcept;

  private:
    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point interval_time_{start_time_};
    size_t cumulative_inbound_bytes_{0};
    size_t cumulative_outbound_bytes_{0};
    size_t interval_inbound_bytes_{0};
    size_t interval_outbound_bytes_{0};
};

}  // namespace znode::net
