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
    if (interval_milliseconds_ == 0 or not call_back_) return false;
    if (is_ticking_.load()) return false;
    if (Stoppable::start()) {
        start_internal();
        return true;
    }
    return false;
}

bool AsioTimer::start(uint32_t interval, CallBackFunc call_back) noexcept {
    if (is_ticking_.load()) return false;
    interval_milliseconds_.store(interval);
    call_back_ = std::move(call_back);
    return start();
}

bool AsioTimer::stop(bool wait) noexcept {
    if (Stoppable::stop(wait)) {
        std::ignore = timer_.cancel();
        return true;
    }
    return false;
}

void AsioTimer::start_internal() {
    bool expected{false};
    if (not is_ticking_.compare_exchange_strong(expected, true)) return;

    timer_.expires_after(std::chrono::milliseconds(interval_milliseconds_.load()));
    timer_.async_wait([this](const boost::system::error_code& error_code) {
        is_ticking_.store(false);
        if (error_code) {
            auto severity{error_code == boost::asio::error::operation_aborted ? log::Level::kTrace
                                                                              : log::Level::kError};
            log::BufferBase(severity, absl::StrCat("AsioTimer[", name_, "]"),
                            {"action", "async_wait", "error", error_code.message()});
            set_stopped();
            return;
        };
        const auto call_back_result{do_call_back()};
        if (not is_stopping() and call_back_result and autoreset()) {
            start_internal();
        } else {
            set_stopped();
        }
    });
}

bool AsioTimer::do_call_back() {
    bool result{true};
    try {
        const auto new_interval{call_back_(interval_milliseconds_.load())};
        interval_milliseconds_.store(new_interval);
        result or_eq (new_interval == 0U);
    } catch (const std::exception& ex) {
        log::Error(absl::StrCat("AsioTimer[", name_, "]"), {"action", "call_back", "error", ex.what()});
        result = false;
    }
    return result;
}
}  // namespace zenpp
