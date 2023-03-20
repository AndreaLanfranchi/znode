/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/types/block.hpp>

namespace zen {

TEST_CASE("Block Serialization", "[serialization]") {
    BlockHeader header;
    header.version = 15;
    ser::DataStream stream(ser::Scope::kNetwork, 0);
    CHECK(header.serialized_size(stream) == 12);
    stream.clear();
    header.serialize(stream);

    // Check the version equals to 15
    auto version_parsed{endian::load_little_u32(&stream[0])};
    CHECK(version_parsed == 15);
}
}  // namespace zen
