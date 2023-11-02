/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <thread>

#include <catch2/catch.hpp>

#include <infra/common/stopwatch.hpp>

namespace znode {
TEST_CASE("Stop Watch", "[infra][common][stopwatch]") {
    using namespace std::chrono_literals;

    StopWatch sw_autostart(true);
    REQUIRE(sw_autostart);  // Must be started

    StopWatch sw1{};
    REQUIRE(!sw1);  // Not started

    const auto [lap_time0, duration0] = sw1.lap();
    CHECK(duration0.count() == 0);
    CHECK(lap_time0 == StopWatch::TimePoint());

    const auto start_time = sw1.start();
    CHECK(sw1);  // Started

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto [lap_time1, duration1] = sw1.lap();
    CHECK(duration1.count() >= 5 * 1000);
    CHECK(start_time < lap_time1);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto [lap_time2, duration2] = sw1.lap();
    CHECK(duration2.count() >= 10 * 1000);
    CHECK(lap_time1 < lap_time2);

    const auto duration3 = sw1.since_start(lap_time2);
    CHECK(duration3.count() == (duration1.count() + duration2.count()));

    CHECK(sw1.laps().size() == 3);  // Start + 2 laps
    for (const auto& [t, _] : sw1.laps()) {
        CHECK(t >= start_time);
    }

    CHECK_FALSE(sw1.format(duration3).empty());
    CHECK(sw1.format(255h + 12min + 14s) == "10d 15h 12m 14s");
    CHECK(sw1.format(240h) == "10d");
    CHECK(sw1.format(240h + 14s) == "10d 14s");
    CHECK(sw1.format(7min + 12s + 120ms) == "7m 12s");
    CHECK(sw1.format(1ms) == "1ms");
    CHECK(sw1.format(1200ms) == "1.200s");
    CHECK(sw1.format(1010us) == "1.010ms");
    CHECK(sw1.format(20us) == "20us");

    std::ignore = sw1.stop();
    std::ignore = sw1.start(/*with_reset=*/true);
    CHECK(sw1.laps().empty() == false);
    std::ignore = sw1.stop();
    const auto [_, duration4]{sw1.stop()};
    CHECK(duration4.count() == 0);

    sw1.reset();
    CHECK(sw1.laps().empty());  // No more laps
    CHECK_FALSE(sw1);           // Not started
}
}  // namespace znode
