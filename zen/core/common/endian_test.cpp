/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/common/endian.hpp>
#include <zen/core/encoding/hex.hpp>

namespace zen::endian {

TEST_CASE("16 bit", "[endianness]") {
    uint8_t bytes[2];
    uint16_t value{0x1234};

    store_big_u16(bytes, value);
    CHECK(bytes[0] == 0x12);
    CHECK(bytes[1] == 0x34);

    uint16_t be{load_big_u16(bytes)};
    CHECK(be == value);

    uint16_t le{load_little_u16(bytes)};
    CHECK(le == 0x3412);
}

TEST_CASE("32 bit", "[endianness]") {
    uint8_t bytes[4];
    uint32_t value{0x12345678};

    store_big_u32(bytes, value);
    CHECK(bytes[0] == 0x12);
    CHECK(bytes[1] == 0x34);
    CHECK(bytes[2] == 0x56);
    CHECK(bytes[3] == 0x78);

    uint32_t be{load_big_u32(bytes)};
    CHECK(be == value);

    uint32_t le{load_little_u32(bytes)};
    CHECK(le == 0x78563412);
}

TEST_CASE("64 bit", "[endianness]") {
    uint8_t bytes[8];
    uint64_t value{0x123456789abcdef0};

    store_big_u64(bytes, value);
    CHECK(bytes[0] == 0x12);
    CHECK(bytes[1] == 0x34);
    CHECK(bytes[2] == 0x56);
    CHECK(bytes[3] == 0x78);
    CHECK(bytes[4] == 0x9a);
    CHECK(bytes[5] == 0xbc);
    CHECK(bytes[6] == 0xde);
    CHECK(bytes[7] == 0xf0);

    uint64_t be{load_big_u64(bytes)};
    CHECK(be == value);

    uint64_t le{load_little_u64(bytes)};
    CHECK(le == 0xf0debc9a78563412);
}

TEST_CASE("Byte swapping", "[endianness]") {
    uint64_t value{0x123456789abcdef0};
    CHECK(hex::encode(value, true) == "0x123456789abcdef0");
    auto value_swapped = byte_swap(value);
    CHECK(hex::encode(value_swapped, true) == "0xf0debc9a78563412");
    CHECK(hex::reverse_hex(hex::encode(value, true)) == "0xf0debc9a78563412");
}

}  // namespace zen::endian
