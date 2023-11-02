/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "stoppable.hpp"

namespace znode::con {

bool Stoppable::start() noexcept {
    ComponentStatus expected{ComponentStatus::kNotStarted};
    return state_.compare_exchange_strong(expected, ComponentStatus::kStarted);
}

bool Stoppable::stop() noexcept {
    if (status() == ComponentStatus::kNotStarted) return false;
    ComponentStatus expected{ComponentStatus::kStarted};
    return state_.compare_exchange_strong(expected, ComponentStatus::kStopping);
}

Stoppable::ComponentStatus Stoppable::status() const noexcept { return state_.load(); }

bool Stoppable::is_running() const noexcept { return status() == ComponentStatus::kStarted; }

void Stoppable::set_stopped() noexcept { state_.exchange(ComponentStatus::kNotStarted); }

}  // namespace znode::con
