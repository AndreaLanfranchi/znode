/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/common/time.hpp>

namespace zenpp::time {

int64_t now() noexcept {
    const auto tnow{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
    return static_cast<int64_t>(tnow);
}
}  // namespace zenpp::time
