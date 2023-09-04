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
    is_ticking_.store(true);
    timer_.expires_after(std::chrono::milliseconds(interval_milliseconds_.load()));
    timer_.async_wait([this](const boost::system::error_code& error_code) {
        is_ticking_.store(false);
        const auto call_back_result{do_call_back(error_code)};
        if (call_back_result and autoreset()) {
            start_internal();
        } else {
            set_stopped();
        }
    });
}

bool AsioTimer::do_call_back(boost::system::error_code error_code) {
    bool result{error_code not_eq boost::asio::error::operation_aborted and not is_stopping()};
    if (result) {
        try {
            if (error_code) throw std::runtime_error(error_code.what());
            const auto new_interval{call_back_(interval_milliseconds_.load())};
            interval_milliseconds_.store(new_interval);
            result or_eq (new_interval == 0U);
        } catch (const std::exception& ex) {
            log::Error(absl::StrCat("AsioTimer[", name_, "]"), {"action", "call_back", "error", ex.what()});
            result = false;
        }
    }
    result or_eq is_stopping();
    return result;
}
}  // namespace zenpp
