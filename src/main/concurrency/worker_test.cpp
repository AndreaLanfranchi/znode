/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <zen/node/common/log_test.hpp>
#include <zen/node/concurrency/worker.hpp>

namespace zen {

class TestWorker final : public Worker {
  public:
    explicit TestWorker(bool should_throw = false) : Worker("testworker"), should_throw_(should_throw){};
    ~TestWorker() override = default;
    uint32_t get_increment() const { return increment_; }

  private:
    std::atomic_bool should_throw_;
    std::atomic_uint32_t increment_{0};
    void work() override {
        while (wait_for_kick()) {
            ++increment_;
            if (should_throw_) {
                throw std::runtime_error("An exception");
            }
        }
    }
};

std::mutex kick_mtx;
std::condition_variable kick_cv;

void trace_worker_state_changes(const Worker* origin) {
    auto new_state{origin->state()};
    std::ignore = log::Trace("Worker state changed", {"name", origin->name(), "id", std::to_string(origin->id()),
                                                      "state", std::string(magic_enum::enum_name(new_state))});
    if (new_state == Worker::State::kKickWaiting) kick_cv.notify_one();
}

TEST_CASE("Threaded Worker", "[concurrency]") {
    using namespace std::placeholders;
    using enum Worker::State;

    log::SetLogVerbosityGuard log_guard(log::Level::kTrace);

    SECTION("No throw") {
        TestWorker worker(false);
        auto connection = worker.signal_worker_state_changed.connect(std::bind(&trace_worker_state_changes, _1));
        CHECK(worker.state() == kStopped);
        worker.start(false, true);

        {
            std::unique_lock l(kick_mtx);
            kick_cv.wait(l);
        }
        CHECK(worker.get_increment() == 0);

        worker.kick();
        {
            std::unique_lock l(kick_mtx);
            kick_cv.wait(l);
        }
        CHECK(worker.get_increment() == 1);

        worker.kick();
        {
            std::unique_lock l(kick_mtx);
            kick_cv.wait(l);
        }
        CHECK(worker.get_increment() == 2);

        worker.stop(true);
        CHECK(worker.state() == kStopped);
        connection.disconnect();
    }

    SECTION("Throw") {
        TestWorker worker(true);
        CHECK(worker.state() == kStopped);
        worker.start(true, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.state() == kStopped);
        CHECK(worker.has_exception() == true);
        CHECK_THROWS(worker.rethrow());
    }

    SECTION("Stop when already exited") {
        TestWorker worker(true);
        CHECK(worker.state() == kStopped);
        worker.start(true, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CHECK(worker.state() == kStopped);  // likely
        worker.stop(true);
        CHECK(worker.state() == kStopped);
    }
}
}  // namespace zen
