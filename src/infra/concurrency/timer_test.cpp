/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "timer.hpp"

#include <atomic>
#include <condition_variable>

#include <boost/asio/io_context.hpp>
#include <catch2/catch.hpp>

#include <infra/common/log.hpp>

namespace zenpp::con {

using namespace std::chrono_literals;

TEST_CASE("Async Timer", "[infra][concurrency][timer]") {
    boost::asio::io_context context;
    std::atomic_uint32_t counter{0};
    const auto call_back = [&counter](std::chrono::milliseconds& interval) {
        LOG_INFO << "Timer triggered after " << interval;
        if (++counter == 10) throw std::runtime_error("Test exception");
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
        std::chrono::milliseconds interval{10};
        Timer test_timer(context, "test_timer", interval, call_back);
        test_timer.start();
        context.run_for(interval * 5);  // This is blocking
        REQUIRE(counter == 1);
    }

    SECTION("Timer with autoreset") {
        std::chrono::milliseconds interval{100};
        Timer test_timer(context, "test_timer", interval, call_back, true);
        test_timer.start();
        context.run_for((interval * 5) - 50ms);  // This is blocking
        REQUIRE(counter == 4);
    }

    SECTION("Timer with callback throwing exception") {
        std::chrono::milliseconds interval{10};
        Timer test_timer(context, "test_timer", interval, call_back, true);
        test_timer.start();
        context.run_for(500ms);  // This is blocking
        REQUIRE(test_timer.has_exception());
    }
}
}  // namespace zenpp::con
