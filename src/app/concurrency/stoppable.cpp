/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/concurrency/stoppable.hpp>

namespace zenpp {

bool Stoppable::start() noexcept {
    bool expected{false};
    return started_.compare_exchange_strong(expected, true);
}

bool Stoppable::stop([[maybe_unused]] /*in non-threaded components we don't need this*/ bool wait) noexcept {
    bool expected{false};
    return stop_requested_.compare_exchange_strong(expected, true);
}

bool Stoppable::is_stopping() const noexcept { return stop_requested_; }

bool Stoppable::is_running() const noexcept { return started_ and not stop_requested_; }

void Stoppable::set_stopped() noexcept {
    started_.store(false);
    stop_requested_.store(false);
}
}  // namespace zenpp
