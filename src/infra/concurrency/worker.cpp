/*
   Copyright 2022 The Silkworm Authors
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

#include "worker.hpp"

#include <core/common/assert.hpp>

#include <infra/common/log.hpp>

namespace znode::con {

Worker::~Worker() { stop(); }

bool Worker::start() noexcept {
    const bool start_already_requested{not Stoppable::start()};
    if (start_already_requested) return false;

    exception_ptr_ = nullptr;
    kicked_.store(false);
    id_.store(0);

    boost::thread::attributes attrs;
    if (stack_size_.has_value()) {
        attrs.set_stack_size(stack_size_.value());
    }

    thread_ = std::make_unique<boost::thread>(attrs, [this]() {
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

        set_stopped();
        kicked_.exchange(false);
        id_.store(0);
    });

    return true;
}

bool Worker::stop() noexcept {
    const bool ret{Stoppable::stop()};
    if (ret) kick();
    if (thread_ not_eq nullptr) {
        // Worker thread cannot call stop on itself
        // It has to exit the work() function to be stopped
        if (id_ not_eq 0U and id_ == log::get_thread_id()) {
            std::ignore = log::Error("Worker::stop() called from worker thread",
                                     {"name", name_, "id", std::to_string(id_.load())});
            ASSERT(false);
        }

        if (thread_->joinable()) thread_->join();
        thread_.reset();
    }
    return ret;
}

void Worker::kick() {
    kicked_.store(true);
    kicked_cv_.notify_one();
}

bool Worker::wait_for_kick(uint32_t timeout_milliseconds) {
    bool expected_kicked_value{true};
    while (not kicked_.compare_exchange_strong(expected_kicked_value, false)) {
        if (timeout_milliseconds not_eq 0U) {
            std::unique_lock lock(kick_mtx_);
            std::ignore = kicked_cv_.wait_for(lock, std::chrono::milliseconds(timeout_milliseconds));
        } else {
            std::this_thread::yield();
        }
        if (not is_running()) return false;  // Might have been a kick to stop
        expected_kicked_value = true;        // !!Important - reset the expected value
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
}  // namespace znode::con
