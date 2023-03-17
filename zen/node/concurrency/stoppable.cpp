/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "stoppable.hpp"

#include <zen/node/concurrency/ossignals.hpp>

namespace zen {

bool Stoppable::stop(bool wait) noexcept {
    std::ignore = wait;  // In non threaded components we don't need this
    bool expected{false};
    return stop_requested_.compare_exchange_strong(expected, true);
}

bool Stoppable::is_stopping() const noexcept { return stop_requested_ || Ossignals::signalled(); }

}  // namespace zen
