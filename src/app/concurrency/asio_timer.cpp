/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <utility>

#include <absl/strings/str_cat.h>

#include <app/common/log.hpp>
#include <app/concurrency/asio_timer.hpp>

namespace zenpp {
bool AsioTimer::start() noexcept {
    if (interval_milliseconds_ == 0U or not call_back_) return false;
    if (Stoppable::start()) {
        start_internal();
        return true;
    }
    set_stopped();
    return false;
}

bool AsioTimer::start(uint32_t interval, CallBackFunc call_back) noexcept {
    interval_milliseconds_.store(interval);
    call_back_ = std::move(call_back);
    return start();
}

bool AsioTimer::stop(bool wait) noexcept {
    if (Stoppable::stop(wait)) {
        std::ignore = timer_.cancel();
        set_stopped();
        return true;
    }
    return false;
}

void AsioTimer::start_internal() {
    timer_.expires_after(std::chrono::milliseconds(interval_milliseconds_.load()));
    timer_.async_wait([this](const boost::system::error_code& error_code) {
        if (error_code) {
            //if (error_code not_eq boost::asio::error::operation_aborted) {
                auto severity{error_code == boost::asio::error::operation_aborted ? log::Level::kTrace
                                                                                  : log::Level::kError};
                log::BufferBase(severity, absl::StrCat("AsioTimer[", name_, "]"),
                                {"action", "async_wait", "error", error_code.message()});
            //}
            set_stopped();
            return;
        }
        try {
            const auto new_interval_milliseconds{call_back_(interval_milliseconds_)};
            if (is_running() and autoreset() and new_interval_milliseconds > 0U) {
                interval_milliseconds_.store(new_interval_milliseconds);
                start_internal();
            } else {
                set_stopped();
            }
        } catch (const std::exception& ex) {
            std::ignore =
                log::Error(absl::StrCat("AsioTimer[", name_, "]"), {"action", "timer_expire", "error", ex.what()});
            set_stopped();
        }
    });
}
}  // namespace zenpp
