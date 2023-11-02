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
