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

#include "stopwatch.hpp"

#include <tuple>

namespace znode {

StopWatch::TimePoint StopWatch::start(bool with_reset) noexcept {
    using namespace std::chrono_literals;
    if (with_reset) reset();
    if (started_) return start_time_;

    started_ = true;
    if (start_time_ == TimePoint()) start_time_ = TimeClock::now();
    if (not laps_.empty()) {
        const auto& [time_point, duration] = laps_.back();
        laps_.emplace_back(start_time_, std::chrono::duration_cast<Duration>(start_time_ - time_point));
    } else {
        laps_.emplace_back(start_time_, std::chrono::duration_cast<Duration>(0s));
    }
    return start_time_;
}

std::pair<StopWatch::TimePoint, StopWatch::Duration> StopWatch::lap() noexcept {
    if (not started_ or laps_.empty()) return {};
    const auto lap_time{TimeClock::now()};
    const auto& [time_point, duration] = laps_.back();
    laps_.emplace_back(lap_time, std::chrono::duration_cast<Duration>(lap_time - time_point));
    return laps_.back();
}

StopWatch::Duration StopWatch::since_start(const TimePoint& origin) const noexcept {
    if (start_time_ == TimePoint()) {
        return {};
    }
    return Duration(origin - start_time_);
}

StopWatch::Duration StopWatch::since_start() const noexcept { return since_start(TimeClock::now()); }

std::pair<StopWatch::TimePoint, StopWatch::Duration> StopWatch::stop() noexcept {
    if (!started_) {
        return {};
    }
    auto ret{lap()};
    started_ = false;
    return ret;
}

void StopWatch::reset() noexcept {
    std::ignore = stop();
    start_time_ = TimePoint();
    if (not laps_.empty()) {
        std::vector<std::pair<TimePoint, Duration>>().swap(laps_);
    }
}

std::string StopWatch::format(Duration duration) noexcept {
    using namespace std::chrono_literals;
    using days = std::chrono::duration<int, std::ratio<86400>>;

    std::ostringstream ostream;
    const char fill = ostream.fill('0');

    if (duration >= 60s) {
        bool need_space{false};
        if (auto d = std::chrono::duration_cast<days>(duration); d.count() not_eq 0) {
            ostream << d.count() << "d";
            duration -= d;
            need_space = true;
        }
        if (auto h = std::chrono::duration_cast<std::chrono::hours>(duration); h.count() not_eq 0) {
            ostream << (need_space ? " " : "") << h.count() << "h";
            duration -= h;
            need_space = true;
        }
        if (auto m = std::chrono::duration_cast<std::chrono::minutes>(duration); m.count() not_eq 0) {
            ostream << (need_space ? " " : "") << m.count() << "m";
            duration -= m;
            need_space = true;
        }
        if (auto s = std::chrono::duration_cast<std::chrono::seconds>(duration); s.count() not_eq 0) {
            ostream << (need_space ? " " : "") << s.count() << "s";
        }
    } else {
        if (duration >= 1s) {
            auto s = std::chrono::duration_cast<std::chrono::seconds>(duration);
            duration -= s;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            ostream << s.count();
            if (ms.count() not_eq 0) {
                ostream << "." << std::setw(3) << ms.count();
            }
            ostream << "s";
        } else if (duration >= 1ms) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            duration -= ms;
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration);
            ostream << ms.count();
            if (us.count() not_eq 0) {
                ostream << "." << std::setw(3) << us.count();
            }
            ostream << "ms";
        } else if (duration >= 1us) {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration);
            ostream << us.count() << "us";
        } else if (duration >= 1ns) {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            ostream << ns.count() << "ns";
        } else {
            ostream << "nil";
        }
    }

    ostream.fill(fill);
    return ostream.str();
}

}  // namespace znode
