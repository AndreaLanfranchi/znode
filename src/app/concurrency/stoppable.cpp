/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/concurrency/stoppable.hpp>

namespace zenpp {

bool Stoppable::start() noexcept {
    State expected{State::kNotStarted};
    return state_.compare_exchange_strong(expected, State::kStarted);
}

bool Stoppable::stop([[maybe_unused]] /*in non-threaded components we don't need this*/ bool wait) noexcept {
    State expected{State::kStarted};
    return state_.compare_exchange_strong(expected, State::kStopping);
}

Stoppable::State Stoppable::state() const noexcept { return state_.load(std::memory_order_acquire); }

bool Stoppable::is_stopping() const noexcept { return state() == State::kStopping; }

void Stoppable::set_stopped() noexcept { state_.store(State::kNotStarted, std::memory_order_release); }
}  // namespace zenpp
