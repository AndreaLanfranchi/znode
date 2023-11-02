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

#include <atomic>
#include <thread>

#include <catch2/catch.hpp>

#include <infra/common/log.hpp>
#include <infra/common/log_test.hpp>
#include <infra/concurrency/context.hpp>
#include <infra/concurrency/timer.hpp>

namespace znode::con {

using namespace std::chrono_literals;

TEST_CASE("Async Timer1", "[infra][concurrency][timer]") {
    log::SetLogVerbosityGuard guard(log::Level::kTrace);
    con::Context context("test");
    context.start();
    std::atomic_uint32_t counter{0};
    std::atomic_bool should_throw{false};

    const auto call_back = [&counter, &should_throw](Timer::duration& interval) {
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
        CHECK_FALSE(test_timer.start(0s, call_back));
    }

    SECTION("Timer with no autoreset") {
        Timer::duration interval{100};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/false);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 5);
        REQUIRE(counter == 1);
    }

    SECTION("Timer with autoreset") {
        Timer::duration interval{50};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 5);
        context.stop();
        REQUIRE(counter == 4);
    }

    SECTION("Timer with callback throwing exception") {
        should_throw = true;
        Timer::duration interval{50};
        Timer test_timer(*context, "test_timer", interval, call_back, /*autoreset=*/true);
        CHECK(test_timer.start());
        std::this_thread::sleep_for(interval * 15);
        REQUIRE(test_timer.has_exception());
    }
}
}  // namespace znode::con
