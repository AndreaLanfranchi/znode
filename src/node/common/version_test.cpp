/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <node/common/version.hpp>

namespace znode {
TEST_CASE("Versions", "[misc]") {
    const Version ver0{};
    const Version ver1{0, 0, 1};
    REQUIRE(ver0 < ver1);
    REQUIRE(ver1 > ver0);
    REQUIRE(ver0.to_string() == "0.0.0");
    REQUIRE(ver1.to_string() == "0.0.1");
}
}  // namespace znode
