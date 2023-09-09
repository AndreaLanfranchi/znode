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

Worker::~Worker() { Worker::stop(/*wait=*/true); }

bool Worker::start() noexcept {
    const bool start_already_requested{!Stoppable::start()};
    if (start_already_requested) return false;
    state_.store(State::kStarting);
    signal_worker_state_changed(this);

    exception_ptr_ = nullptr;
    id_.store(0);

    thread_ = std::make_unique<std::thread>([this]() {
        log::set_thread_name(name_);

        // Retrieve the id
        id_.store(log::get_thread_id());

        if (State expected_starting{State::kStarting};
            state_.compare_exchange_strong(expected_starting, State::kStarted)) {
            thread_started_cv_.notify_one();
            signal_worker_state_changed(this);
            try {
                work();
            } catch (const std::exception& ex) {
                std::ignore = log::Error("Worker error", {"name", name_, "id", std::to_string(id_.load()), "exception",
                                                          std::string(ex.what())});
                exception_ptr_ = std::current_exception();
            } catch (...) {
                std::ignore = log::Error(
                    "Worker error", {"name", name_, "id", std::to_string(id_.load()), "exception", "Undefined error"});
            }
        }
        state_.store(State::kStopped);
        signal_worker_state_changed(this);
        id_.store(0);
    });

    while (true) {
        std::unique_lock lock(kick_mtx_);
        if (thread_started_cv_.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::no_timeout) {
            break;
        }
    }
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
        // We've NOT been kicked yet hence either
        // 1) We're stopping therefore we stop waiting and immediately return false
        // 2) We change the state in kKickWaiting and begin to wait
        if (is_stopping()) break;

        if (State expected_state{State::kStarted};
            state_.compare_exchange_strong(expected_state, State::kKickWaiting)) {
            signal_worker_state_changed(this);
        }
        if (timeout_milliseconds > 0) {
            std::unique_lock lock(kick_mtx_);
            std::ignore = kicked_cv_.wait_for(lock, std::chrono::milliseconds(timeout_milliseconds));
        } else {
            std::this_thread::yield();
        }

        expected_kicked_value = true;  // !!Important
    }

    if (is_stopping()) {
        state_.store(State::kStopping);
        signal_worker_state_changed(this);
        return false;
    }
    state_.store(State::kStarted);
    signal_worker_state_changed(this);
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
