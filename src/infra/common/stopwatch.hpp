/*
   Copyright 2022 The Silkworm Authors
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
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace znode {
//! \brief Mimics the behavior of a wrist stopwatch. Used to measure timings of operations.
class StopWatch {
  public:
    using TimeClock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<TimeClock>;
    using Duration = std::chrono::nanoseconds;

    //! \brief Creates a new instance
    explicit StopWatch(bool auto_start = false) {
        if (auto_start) start();
    };
    ~StopWatch() = default;

    //! \brief Starts the clock
    //! \return The TimePoint it was started on
    TimePoint start(bool with_reset = false) noexcept;

    //! \brief Records a lap time
    //! \return A pair of TimePoint and Duration
    std::pair<TimePoint, Duration> lap() noexcept;

    //! \brief Computes the duration amongst the start time and the provided timepoint
    //! \param origin [in] : An origin timepoint
    //! \return  Duration
    [[nodiscard]] Duration since_start(const TimePoint& origin) const noexcept;

    //! \brief Computes the duration amongst now and the start time
    //! \return  Duration
    [[nodiscard]] Duration since_start() const noexcept;

    //! \brief Stops the watch
    //! \return The timepoint of stop and the duration since start (if no laptimes) or the duration from previous
    //! laptime
    std::pair<TimePoint, Duration> stop() noexcept;

    //! \brief Stops the watch and clears all counters
    void reset() noexcept;

    //! \brief Returns the vector of laptimes
    [[nodiscard]] const std::vector<std::pair<TimePoint, Duration>>& laps() const { return laps_; }

    //! \brief Returns a human readable duration
    static std::string format(Duration duration) noexcept;

    explicit operator bool() const noexcept { return started_; }

  private:
    bool started_{false};
    TimePoint start_time_{};
    std::vector<std::pair<TimePoint, Duration>> laps_{};
};
}  // namespace znode
