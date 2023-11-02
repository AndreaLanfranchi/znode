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

#include "channel.hpp"

#include "context.hpp"

#include <catch2/catch.hpp>

#include <infra/common/log_test.hpp>

namespace znode::con {

TEST_CASE("Channel", "[infra][concurrency][channel]") {
    Context context("test");
    context.start();
    Channel<int> channel(context->get_executor());
    std::vector<int> values{1, 2, 3, 4, 5};
    for (auto& value : values) {
        CHECK_FALSE(channel.try_send(value));
    }
}
}  // namespace znode::con
