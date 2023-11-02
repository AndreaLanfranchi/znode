/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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

StopWatch::Duration StopWatch::since_start(const TimePoint& origin) noexcept {
    if (start_time_ == TimePoint()) {
        return {};
    }
    return Duration(origin - start_time_);
}

StopWatch::Duration StopWatch::since_start() noexcept { return since_start(TimeClock::now()); }

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
        }
    }

    ostream.fill(fill);
    return ostream.str();
}

}  // namespace znode
