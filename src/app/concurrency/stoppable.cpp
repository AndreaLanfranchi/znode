/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <app/concurrency/ossignals.hpp>
#include <app/concurrency/stoppable.hpp>

namespace zenpp {

bool Stoppable::stop([[maybe_unused]] /*in non-threaded components we don't need this*/ bool wait) noexcept {
    bool expected{false};
    return stop_requested_.compare_exchange_strong(expected, true);
}

bool Stoppable::is_stopping() const noexcept { return stop_requested_ || Ossignals::signalled(); }

}  // namespace zenpp
