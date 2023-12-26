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

void Stoppable::wait_stopped() noexcept {
    std::unique_lock lock{component_stopped_mutex_};
    component_stopped_cv_.wait(lock, [this]() { return status() == ComponentStatus::kNotStarted; });
}

Stoppable::ComponentStatus Stoppable::status() const noexcept { return state_.load(); }

bool Stoppable::is_running() const noexcept { return status() == ComponentStatus::kStarted; }

void Stoppable::set_stopped() noexcept {
    state_.exchange(ComponentStatus::kNotStarted);
    component_stopped_cv_.notify_all();
}
}  // namespace znode::con
