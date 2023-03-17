/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "worker.hpp"

#include <zen/node/common/log.hpp>

namespace zen {

Worker::~Worker() { stop(/*wait=*/true); }

void Worker::start(bool kicked, bool wait) noexcept {
    if (State expected_state{State::kStopped}; !state_.compare_exchange_strong(expected_state, State::kStarting)) {
        return;
    }
    signal_worker_state_changed(this);

    exception_ptr_ = nullptr;
    kicked_.store(kicked);
    id_.store(0);

    thread_ = std::make_unique<std::thread>([&]() {
        log::set_thread_name(name_.c_str());

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
            }
        }
        state_.store(State::kStopped);
        signal_worker_state_changed(this);
        id_.store(0);
    });

    while (wait) {
        std::unique_lock lock(kick_mtx_);
        if (thread_started_cv_.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::no_timeout) {
            break;
        }
    }
}
bool Worker::stop(bool wait) noexcept {
    bool already_requested{!Stoppable::stop(wait)};  // Sets stop_requested_ == true;
    if (!already_requested) kick();
    if (wait && thread_) {
        thread_->join();
        thread_.reset();
    }
    return !already_requested && wait;
}

void Worker::kick() {
    kicked_.store(true);
    kicked_cv_.notify_one();
}

bool Worker::wait_for_kick(uint32_t timeout_milliseconds) {
    bool expected_kicked_value{true};
    while (!kicked_.compare_exchange_strong(expected_kicked_value, false)) {
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

}  // namespace zen
