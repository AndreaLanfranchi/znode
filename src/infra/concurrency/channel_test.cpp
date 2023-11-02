/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
