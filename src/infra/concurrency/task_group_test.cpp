/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <chrono>
#include <stdexcept>

#include <boost/asio/steady_timer.hpp>
#include <catch2/catch.hpp>

#include <infra/concurrency/context.hpp>
#include <infra/concurrency/task_group.hpp>

namespace zenpp::con {
using namespace boost;
using namespace std::chrono_literals;
namespace {

    class TestException : public std::runtime_error {
      public:
        TestException() : std::runtime_error("TestException") {}
    };

    Task<void> async_no_throw() { co_await ThisTask::executor; }

    Task<void> async_throw() {
        co_await ThisTask::executor;
        throw TestException();
    }

    Task<void> wait_until_cancelled(bool& cancelled) {
        try {
            auto executor = co_await ThisTask::executor;
            asio::steady_timer timer(executor);
            timer.expires_from_now(1h);
            co_await timer.async_wait(asio::use_awaitable);
        } catch (const boost::system::system_error error) {
            cancelled = true;
        }
    }

    Task<void> sleep(std::chrono::milliseconds duration) {
        auto executor = co_await ThisTask::executor;
        asio::steady_timer timer(executor);
        timer.expires_after(duration);
        co_await timer.async_wait(asio::use_awaitable);
    }

}  // namespace

//TEST_CASE("TaskGroup 0", "[infra][concurrency][taskgroup]") {
//    Context test_context("Test", 1);
//    test_context.start();
//    TaskRunner runner;
//    TaskGroup test_group(test_context->get_executor(), 0);
//    test_group.spawn(test_context->get_executor(), async_no_throw());
//    test_group.spawn(test_context->get_executor(), async_throw());
//    CHECK_THROWS_AS(runner.run(test_group.wait()), TestException);
//}

}  // namespace zenpp::con
