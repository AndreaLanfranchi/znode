/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/types/block.hpp>

namespace znode {

TEST_CASE("Block Serialization", "[serialization]") {
    BlockHeader header;
    header.version = 15;
    header.parent_hash = h256(10);
    ser::SDataStream stream(ser::Scope::kNetwork, 0);
    CHECK(header.serialized_size(stream) == kBlockHeaderSerializedSize);
    stream.clear();
    REQUIRE_FALSE(header.serialize(stream).has_error());

    BlockHeader header2;
    REQUIRE_FALSE(header2.deserialize(stream).has_error());
    CHECK(header == header2);
    CHECK(stream.eof());
}
}  // namespace znode
