/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "timer.hpp"

#include "timer2.hpp"

#include <atomic>
#include <condition_variable>

#include <boost/asio/io_context.hpp>
#include <catch2/catch.hpp>

#include <infra/common/log.hpp>

namespace zenpp::con {

using namespace std::chrono_literals;

TEST_CASE("Async Timer1", "[infra][concurrency][timer]") {
    boost::asio::io_context context;
    std::atomic_uint32_t counter{0};
    std::atomic_bool should_throw{false};

    const auto call_back = [&counter, &should_throw](std::chrono::milliseconds& interval) {
        LOG_INFO << "Timer triggered after " << interval;
        if (++counter == 10) {
            if (should_throw) throw std::runtime_error("Test exception");
            interval = 0ms;
        }
    };

    SECTION("Timer with no callback") {
        Timer test_timer(context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(100ms, nullptr));
    }
    SECTION("Timer with no interval") {
        Timer test_timer(context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(0ms, call_back));
    }

    SECTION("Timer with no autoreset") {
        std::chrono::milliseconds interval{100};
        Timer test_timer(context, "test_timer", interval, call_back, /*autoreset=*/false);
        CHECK(test_timer.start());
        context.run_for(interval * 5);  // This is blocking
        REQUIRE(counter == 1);
    }

    SECTION("Timer with autoreset") {
        std::chrono::milliseconds interval{50};
        Timer test_timer(context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        context.run_for((interval * 5));  // This is blocking
        REQUIRE(counter == 4);
    }

    SECTION("Timer with callback throwing exception") {
        should_throw = true;
        std::chrono::milliseconds interval{50};
        Timer test_timer(context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        context.run_for(interval /*enough to throw*/ * 15);  // This is blocking
        REQUIRE(test_timer.has_exception());
    }
}
TEST_CASE("Async Timer2", "[.][infra][concurrency][timer]") {
    boost::asio::io_context context;
    std::atomic_uint32_t counter{0};
    std::atomic_bool should_throw{false};

    auto call_back = [&](std::chrono::milliseconds& interval) {
        LOG_INFO << "Timer triggered after " << interval;
        if (++counter == 10) {
            if (should_throw) throw std::runtime_error("Test exception");
            interval = 0ms;
        }
    };

    SECTION("Timer with no callback") {
        Timer2 test_timer(context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(100ms, nullptr));
    }
    SECTION("Timer with no interval") {
        Timer2 test_timer(context, "test_timer");
        CHECK_FALSE(test_timer.start());
        CHECK_FALSE(test_timer.start(0ms, call_back));
    }

    SECTION("Timer with no autoreset") {
        std::chrono::milliseconds interval{100};
        Timer2 test_timer(context, "test_timer", interval, call_back, /*autoreset=*/false);
        CHECK(test_timer.start());
        context.run_for(interval * 2);  // This is blocking
        REQUIRE(counter == 1);
    }

    SECTION("Timer with autoreset") {
        std::chrono::milliseconds interval{50};
        Timer2 test_timer(context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        context.run_for((interval * 11));  // This is
        REQUIRE(counter == 4);
    }

    SECTION("Timer with callback throwing exception") {
        should_throw = true;
        std::chrono::milliseconds interval{100};
        Timer2 test_timer(context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        context.run_for(interval * 11);  // This is blocking
        REQUIRE(test_timer.has_exception());
    }
}
}  // namespace zenpp::con
