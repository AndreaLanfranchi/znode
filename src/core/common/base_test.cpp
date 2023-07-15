/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/common/base.hpp>

namespace zen {

TEST_CASE("Byteviews") {
    Bytes source{'0', '1', '2'};
    ByteView bv1(source);
    bv1.remove_prefix(3);
    REQUIRE(bv1.empty());
    ByteView bv2{};
    REQUIRE(bv2.empty());
    REQUIRE(bv1 == bv2);
    REQUIRE_FALSE(bv1.data() == bv2.data());
    REQUIRE(bv2.is_null());
}

}  // namespace zen
