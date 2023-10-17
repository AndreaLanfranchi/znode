/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <atomic>
#include <condition_variable>
#include <thread>

#include <catch2/catch.hpp>

#include <infra/common/log.hpp>
#include <infra/common/log_test.hpp>
#include <infra/concurrency/context.hpp>
#include <infra/concurrency/timer.hpp>

namespace zenpp::con {

using namespace std::chrono_literals;

TEST_CASE("Async Timer1", "[infra][concurrency][timer]") {
    log::SetLogVerbosityGuard guard(log::Level::kTrace);
    con::Context context("test");
    context.start();
    std::atomic_uint32_t counter{0};
    std::atomic_bool should_throw{false};

    const auto call_back = [&counter, &should_throw](std::chrono::milliseconds& interval) {
        LOG_TRACE << "Timer triggered after " << interval;
        if (++counter == 10) {
            if (should_throw) throw std::runtime_error("Test exception");
            interval = 0ms;
        }
    };

    SECTION("Timer with no callback") {
        Timer test_timer(*context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(100ms, nullptr));
    }
    SECTION("Timer with no interval") {
        Timer test_timer(*context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(0ms, call_back));
    }

    SECTION("Timer with no autoreset") {
        std::chrono::milliseconds interval{100};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/false);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 5);
        REQUIRE(counter == 1);
    }

    SECTION("Timer with autoreset") {
        std::chrono::milliseconds interval{50};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 5);
        context.stop();
        REQUIRE(counter == 4);
    }

    SECTION("Timer with callback throwing exception") {
        should_throw = true;
        std::chrono::milliseconds interval{50};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 15);
        REQUIRE(test_timer.has_exception());
    }
}
}  // namespace zenpp::con
