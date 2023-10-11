/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <infra/common/log_test.hpp>
#include <infra/concurrency/worker.hpp>

namespace zenpp::con {

class TestWorker final : public Worker {
  public:
    explicit TestWorker(bool should_throw = false) : Worker("testworker"), should_throw_(should_throw){};
    ~TestWorker() override = default;
    uint32_t get_increment() const { return increment_.load(); }

  private:
    std::atomic_bool should_throw_;
    std::atomic_uint32_t increment_{0};
    void work() override {
        while (wait_for_kick(10)) {
            ++increment_;
            if (should_throw_) {
                throw std::runtime_error("An exception");
            }
        }
    }
};

TEST_CASE("Threaded Worker", "[concurrency][worker]") {
    using namespace std::placeholders;
    using enum Stoppable::ComponentStatus;

    const log::SetLogVerbosityGuard log_guard(log::Level::kTrace);

    SECTION("No throw") {
        TestWorker worker(/* should_throw=*/false);
        CHECK(worker.status() == kNotStarted);
        worker.start();
        CHECK(worker.status() == kStarted);
        REQUIRE(worker.get_increment() == 0U);
        worker.kick();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.get_increment() == 1U);
        worker.kick();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.get_increment() == 2U);

        worker.stop(true);
        CHECK(worker.status() == kNotStarted);
        CHECK(worker.get_increment() == 2);
    }

    SECTION("Throw") {
        TestWorker worker(true);
        CHECK(worker.status() == kNotStarted);
        worker.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.status() == kStarted);
        CHECK(worker.has_exception() == false);
        worker.kick();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.get_increment() == 1U);
        CHECK(worker.status() == kNotStarted);
        CHECK(worker.has_exception() == true);
        CHECK_THROWS(worker.rethrow());
        CHECK(worker.what() == "An exception");
        worker.stop(true);
    }

    SECTION("Stop when already exited") {
        TestWorker worker(true);
        CHECK(worker.status() == kNotStarted);
        worker.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.status() == kStarted);
        worker.kick();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.status() == kNotStarted);
        CHECK_FALSE(worker.stop(true));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.status() == kNotStarted);
    }
}
}  // namespace zenpp::con
