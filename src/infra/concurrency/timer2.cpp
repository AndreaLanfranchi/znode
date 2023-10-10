/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "timer2.hpp"

#include <thread>
#include <utility>

#include <absl/strings/str_cat.h>

#include <infra/common/log.hpp>

namespace zenpp::con {

using namespace boost;
namespace chrono = std::chrono;

void Timer2::set_callback(Timer2::CallBackFunc call_back) noexcept {
    if (not is_running()) {
        exception_ptr_ = nullptr;
        call_back_ = std::move(call_back);
    }
}

bool Timer2::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started
    if (interval_.load().count() == 0U or not call_back_) return false;
    exception_ptr_ = nullptr;
    LOG_TRACE1 << "Timer[" << name_ << "]: start requested";
    working_.store(true);
    asio::spawn(timer_.get_executor(),
                [this](boost::asio::yield_context yield_context) mutable { this->work(yield_context); });
    return true;
}

bool Timer2::start(chrono::milliseconds interval, CallBackFunc call_back) noexcept {
    if (not is_running()) {
        interval_.store(interval);
        call_back_ = std::move(call_back);
        return start();
    }
    return false;
}

bool Timer2::stop(bool wait) noexcept {
    if (not Stoppable::stop(wait)) return false;  // Already stopped
    LOG_TRACE1 << "Timer[" << name_ << "]: stop requested";
    std::ignore = timer_.cancel();
    if (wait) working_.wait(true);
    LOG_TRACE1 << "Timer[" << name_ << "]: stopped";
    return true;
}

void Timer2::work(const boost::asio::yield_context& yield_context) noexcept {
    try {
        auto wait_interval{interval_.load()};
        const auto resubmit{autoreset_.load()};
        do {
            boost::system::error_code error;
            timer_.expires_after(wait_interval);
            timer_.async_wait(yield_context[error]);
            boost::asio::detail::throw_error(error, "async_wait");

            LOG_TRACE1 << "Timer[" << name_ << "]: expired";
            call_back_(wait_interval);
        } while (is_running() and resubmit and wait_interval.count() not_eq 0U);

    } catch (const system::system_error& error) {
        if (error.code() not_eq asio::error::operation_aborted) {
            std::ignore = log::Error(absl::StrCat("Timer[", name_, "]"),
                                     {"action", "async_wait", "error", error.code().message()});
            exception_ptr_ = std::current_exception();
        }
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
    working_.store(false);
    working_.notify_all();
}

void Timer2::rethrow() const {
    if (exception_ptr_) std::rethrow_exception(exception_ptr_);
}
}  // namespace zenpp::con
