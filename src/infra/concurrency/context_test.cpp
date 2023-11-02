/*
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
