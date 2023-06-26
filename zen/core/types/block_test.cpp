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
    header.parent_hash = h256(10);
    serialization::DataStream archive(serialization::Scope::kNetwork, 0);
    CHECK(header.serialized_size(archive) == kBlockHeaderSerializedSize);
    archive.clear();
    REQUIRE(header.serialize(archive) == serialization::Error::kSuccess);

    BlockHeader header2;
    REQUIRE(header2.deserialize(archive) == serialization::Error::kSuccess);
    CHECK(header == header2);
    CHECK(archive.size() == 0);

    // Check the version equals to 15
    //    auto version_parsed{endian::load_little_u32(&archive[0])};
    //    CHECK(version_parsed == 15);
}
}  // namespace zen
