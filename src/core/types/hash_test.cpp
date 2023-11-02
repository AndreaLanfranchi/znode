/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/types/hash.hpp>

namespace znode {

const std::array<uint8_t, h256::size()> R1Array{0x9c, 0x52, 0x4a, 0xdb, 0xcf, 0x56, 0x11, 0x12, 0x2b, 0x29, 0x12,
                                                0x5e, 0x5d, 0x35, 0xd2, 0xd2, 0x22, 0x81, 0xaa, 0xb5, 0x33, 0xf0,
                                                0x08, 0x32, 0xd5, 0x56, 0xb1, 0xf9, 0xea, 0xe5, 0x1d, 0x7d};
const h256 R1L{{R1Array.data(), h256::size()}};
const h160 R1S{{R1Array.data(), h160::size()}};

TEST_CASE("Hash", "[types]") {
    const h256 hash;
    CHECK_FALSE(hash);  // Is empty

    // Empty hash hex
    std::string expected_hex{"0000000000000000000000000000000000000000000000000000000000000000"};
    std::string out_hex{hash.to_hex()};
    CHECK(out_hex == expected_hex);

    // Exact length valid hex
    std::string input_hex{"0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"};
    auto parsed_hash = h256::from_hex(input_hex);
    REQUIRE_FALSE(parsed_hash.has_error());
    CHECK(parsed_hash.value().data()[0] == 0xc5);
    out_hex = parsed_hash.value().to_hex(/* reverse=*/false, /* with_prefix=*/true);
    CHECK(input_hex == out_hex);

    // Exact length invalid hex
    input_hex = "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85zzzz";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE(parsed_hash.has_error());
    REQUIRE(parsed_hash.error().value() == static_cast<int>(enc::Error::kIllegalHexDigit));

    // Oversize length (Hash loaded but null)
    input_hex = "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470000000";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE_FALSE(parsed_hash.has_error());
    REQUIRE_FALSE(parsed_hash.value().operator bool());  // Is empty

    // Shorter length valid hex
    input_hex = "0x460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE_FALSE(parsed_hash.has_error());
    out_hex = parsed_hash.value().to_hex(/* reverse=*/false, /*with_prefix=*/true);
    CHECK(input_hex != out_hex);
    expected_hex = "0x0000460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470";
    CHECK(out_hex == expected_hex);

    // Comparison
    auto parsed_hash1 = h256::from_hex("0x01");
    REQUIRE_FALSE(parsed_hash1.has_error());
    auto parsed_hash2 = h256::from_hex("0x02");
    REQUIRE_FALSE(parsed_hash2.has_error());
    REQUIRE(parsed_hash1.value() not_eq parsed_hash2.value());
}

TEST_CASE("Hash to jenkins hash", "[types]") {
    auto salt{h256::from_hex("00112233445566778899aabbccddeeff00")};
    REQUIRE_FALSE(salt.has_error());
    const size_t buffer_size{znode::h256::size() / sizeof(uint32_t)};
    std::array<uint32_t, buffer_size> saltbuf{0};
    std::memcpy(&saltbuf, salt.value().data(), Hash<256>::size());

    std::array<uint32_t, buffer_size> r1lbuf{0};
    std::memcpy(&r1lbuf, R1L.data(), h256::size());

    const uint64_t hash1{R1L.hash(salt.value())};
    const uint64_t hash2{crypto::Jenkins::Hash(r1lbuf.data(), buffer_size, saltbuf.data())};
    CHECK(hash1 == hash2);
}
}  // namespace znode
