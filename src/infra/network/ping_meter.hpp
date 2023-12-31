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
#include <cstdint>
#include <mutex>
#include <optional>

namespace znode::net {

//! \brief A simple network ping meter class.
//! \remarks This class is thread-safe.
class PingMeter {
  public:
    //! \brief Instantiates a new PingMeter object.
    //! \param alpha [in] The alpha parameter used to calculate the EMA of ping time across samples
    //! \remarks By default alpha is 0.65. The higher the value, the more weight is given to the most recent samples.
    explicit PingMeter(float alpha = 0.65F);

    //! \brief Begins the recording of a new ping sample.
    //! \remarks If a ping sample is already in progress, the function does nothing.
    void start_sample() noexcept;

    //! \brief Ends the recording of a ping sample.
    //! \remarks If no ping sample is in progress, the function throws.
    void end_sample() noexcept;

    //! \brief Sets the nonce for the next ping sample.
    void set_nonce(uint64_t nonce) noexcept;

    //! \brief Returns the last recorded nonce (if any)
    [[nodiscard]] std::optional<uint64_t> get_nonce() const noexcept;

    //! \brief Returns whether a ping sample is in progress.
    [[nodiscard]] bool pending_sample() const noexcept;

    //! \brief Returns how long the current ping sample has been in progress.
    [[nodiscard]] std::chrono::milliseconds pending_sample_duration() const noexcept;

    //! \brief Returns the EMA of the ping time across samples.
    [[nodiscard]] std::chrono::milliseconds get_ema() const noexcept;

    //! \brief Returns the minimum ping time across samples.
    [[nodiscard]] std::chrono::milliseconds get_min() const noexcept;

    //! \brief Returns the maximum ping time across samples.
    [[nodiscard]] std::chrono::milliseconds get_max() const noexcept;

  private:
    mutable std::mutex mutex_;
    const float alpha_{};
    bool ping_in_progress_{false};
    std::chrono::steady_clock::time_point ping_start_{};
    std::optional<uint64_t> ping_nonce_{std::nullopt};
    std::chrono::milliseconds ping_duration_ms_ema_{0};
    std::chrono::milliseconds ping_duration_ms_min_{0};
    std::chrono::milliseconds ping_duration_ms_max_{0};
};

}  // namespace znode::net
