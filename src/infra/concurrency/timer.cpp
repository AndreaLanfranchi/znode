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

#include "timer.hpp"

#include <utility>

#include <absl/strings/str_cat.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <infra/common/log.hpp>

namespace znode::con {

using namespace boost;
namespace chrono = std::chrono;

void Timer::set_callback(Timer::CallBackFunc call_back) noexcept {
    if (not is_running()) {
        exception_ptr_ = nullptr;
        call_back_ = std::move(call_back);
    }
}

bool Timer::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started
    if (interval_.load().count() == 0U or not call_back_) return false;
    exception_ptr_ = nullptr;
    asio::co_spawn(timer_.get_executor(), work(), asio::detached);
    return true;
}

bool Timer::start(duration interval, CallBackFunc call_back) noexcept {
    if (not is_running()) {
        interval_.store(interval);
        call_back_ = std::move(call_back);
        return start();
    }
    return false;
}

bool Timer::stop() noexcept {
    if (not Stoppable::stop()) return false;  // Already stopped
    std::ignore = timer_.cancel();
    return true;
}

Task<void> Timer::work() {
    try {
        auto wait_interval{interval_.load()};
        const auto resubmit{autoreset_.load()};
        do {
            timer_.expires_after(wait_interval);
            co_await timer_.async_wait(asio::use_awaitable);
            call_back_(wait_interval);
        } while (is_running() and resubmit and wait_interval.count() not_eq 0U);

    } catch (const boost::system::system_error& error) {
        if (error.code() not_eq asio::error::operation_aborted)
            std::ignore = log::Error(absl::StrCat("Timer[", name_, "]"),
                                     {"action", "async_wait", "error", error.code().message()});
    } catch (const std::system_error& exception) {
        std::ignore = log::Critical(absl::StrCat("Timer[", name_, "]"),
                                    {"action", "async_wait", "error", exception.code().message()});
        exception_ptr_ = std::current_exception();
    } catch (const std::exception& exception) {
        std::ignore =
            log::Critical(absl::StrCat("Timer[", name_, "]"), {"action", "callback", "error", exception.what()});
        exception_ptr_ = std::current_exception();
    } catch (...) {
        std::ignore = log::Critical(absl::StrCat("Timer[", name_, "]"), {"action", "callback", "error", "undefined"});
        try {
            throw std::runtime_error("Undefined error");
        } catch (...) {
            exception_ptr_ = std::current_exception();
        }
    }

    set_stopped();
    co_return;
}

void Timer::rethrow() const {
    if (exception_ptr_) std::rethrow_exception(exception_ptr_);
}
}  // namespace znode::con
