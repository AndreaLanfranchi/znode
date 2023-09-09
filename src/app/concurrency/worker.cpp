/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/common/log.hpp>
#include <app/concurrency/worker.hpp>

#include "core/common/assert.hpp"

namespace zenpp {

Worker::~Worker() { Worker::stop(/*wait=*/false); }

bool Worker::start() noexcept {
    const bool start_already_requested{!Stoppable::start()};
    if (start_already_requested) return false;

    exception_ptr_ = nullptr;
    id_.store(0);

    thread_ = std::make_unique<std::thread>([this]() {
        log::set_thread_name(name_);

        // Retrieve the id
        id_.store(log::get_thread_id());

        try {
            work();
        } catch (const std::exception& ex) {
            std::ignore = log::Error(
                "Worker error", {"name", name_, "id", std::to_string(id_.load()), "exception", std::string(ex.what())});
            exception_ptr_ = std::current_exception();
        } catch (...) {
            std::ignore = log::Error("Worker error",
                                     {"name", name_, "id", std::to_string(id_.load()), "exception", "Undefined error"});
        }

        state_.store(State::kStopped);
        id_.store(0);
    });

    return true;
}

bool Worker::stop(bool wait) noexcept {
    // Worker thread cannot call stop on itself
    // It has to exit the work() function to be stopped
    if (thread_->get_id() == std::this_thread::get_id()) {
        std::ignore =
            log::Error("Worker::stop() called from worker thread", {"name", name_, "id", std::to_string(id_.load())});
        ASSERT(false);
    }

    const bool stop_already_requested{not Stoppable::stop(wait)};
    if (stop_already_requested) return false;

    kick();
    if (thread_ not_eq nullptr) {
        if (wait and thread_->joinable()) {
            thread_->join();
            thread_.reset();
        } else {
            thread_->detach();
            thread_.reset();
        }
    }
    return true;
}

void Worker::kick() {
    kicked_.store(true);
    kicked_cv_.notify_one();
}

bool Worker::wait_for_kick(uint32_t timeout_milliseconds) {
    bool expected_kicked_value{true};
    while (not kicked_.compare_exchange_strong(expected_kicked_value, false)) {
        if (timeout_milliseconds > 0U) {
            std::unique_lock lock(kick_mtx_);
            std::ignore = kicked_cv_.wait_for(lock, std::chrono::milliseconds(timeout_milliseconds));
        } else {
            std::this_thread::yield();
        }
        if (is_stopping()) return false;  // Might have been a kick to stop
        expected_kicked_value = true;     // !!Important - reset the expected value
    }
    return true;
}

std::string Worker::what() const noexcept {
    std::string ret{};
    try {
        rethrow();
    } catch (const std::exception& ex) {
        ret = ex.what();
    } catch (const std::string& ex) {
        ret = ex;
    } catch (const char* ex) {
        ret = ex;
    } catch (...) {
        ret = "Undefined error";
    }
    return ret;
}

void Worker::rethrow() const {
    if (has_exception()) {
        std::rethrow_exception(exception_ptr_);
    }
}
}  // namespace zenpp
