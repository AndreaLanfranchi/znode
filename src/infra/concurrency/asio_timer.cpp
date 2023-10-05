/*
Copyright 2022 The Silkworm Authors
Copyright 2023 Horizen Labs
Distributed under the MIT software license, see the accompanying
file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "asio_timer.hpp"

#include <thread>
#include <utility>

#include <absl/strings/str_cat.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <infra/common/log.hpp>

namespace zenpp::con {
bool AsioTimer::start() noexcept {
    if (interval_milliseconds_ == 0U or not call_back_) return false;
    if (Stoppable::start()) {
        LOG_TRACE1 << "Timer[" << name_ << "]: start requested";
        boost::asio::co_spawn(timer_.get_executor(), start_internal(), boost::asio::detached);
        return true;
    }
    return false;
}

bool AsioTimer::start(uint32_t interval, CallBackFunc call_back) noexcept {
    interval_milliseconds_.store(interval);
    call_back_ = std::move(call_back);
    return start();
}

bool AsioTimer::stop(bool wait) noexcept {
    if (Stoppable::stop(wait)) {
        LOG_TRACE1 << "Timer[" << name_ << "]: stop requested";
        std::ignore = timer_.cancel();
        if (wait) {
            while (status() not_eq ComponentStatus::kNotStarted) {
                std::unique_lock lock{stop_mtx_};
                std::ignore = stop_cv_.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
        LOG_TRACE1 << "Timer[" << name_ << "]: stopped";
        return true;
    }
    return false;
}

Task<void> AsioTimer::start_internal() noexcept {
    try {
        auto wait_interval = interval_milliseconds_.load();
        while (is_running() and wait_interval not_eq 0U) {
            timer_.expires_after(std::chrono::milliseconds(wait_interval));
            co_await timer_.async_wait(boost::asio::use_awaitable);
            LOG_TRACE1 << "Timer[" << name_ << "]: expired";
            try {
                wait_interval = call_back_(wait_interval);
                if (not autoreset()) break;
            } catch (const std::exception& exception) {
                log::Critical(absl::StrCat("Timer[", name_, "]"), {"action", "callback", "error", exception.what()});
                break;
            } catch (...) {
                log::Critical(absl::StrCat("Timer[", name_, "]"), {"action", "callback", "error", "undefined"});
                break;
            }
        }
    } catch (const boost::system::system_error& error) {
        if (error.code() not_eq boost::asio::error::operation_aborted) {
            log::Error(absl::StrCat("AsioTimer[", name_, "]"),
                       {"action", "async_wait", "error", error.code().message()});
        }
    }

    set_stopped();
    stop_cv_.notify_all();
    co_return;
}
}  // namespace zenpp::con
