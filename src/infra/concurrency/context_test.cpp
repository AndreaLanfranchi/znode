/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "context.hpp"

#include <chrono>
#include <thread>

#include <catch2/catch.hpp>

#include <infra/common/log_test.hpp>

namespace znode::con {

TEST_CASE("Context", "[infra][concurrency][context]") {
    log::SetLogVerbosityGuard guard(log::Level::kTrace);
    Context context("test", 1);
    REQUIRE(context.start());
    REQUIRE_FALSE(context.start());  // Already started
    std::this_thread::sleep_for(std::chrono::seconds(2));
    REQUIRE(context.stop());
    REQUIRE_FALSE(context.stop());  // Already stopped
}
}  // namespace znode::con
