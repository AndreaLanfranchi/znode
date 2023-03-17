/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/types/hash.hpp>

namespace zen {

const std::array<uint8_t, h256::size()> R1Array{0x9c, 0x52, 0x4a, 0xdb, 0xcf, 0x56, 0x11, 0x12, 0x2b, 0x29, 0x12,
                                                0x5e, 0x5d, 0x35, 0xd2, 0xd2, 0x22, 0x81, 0xaa, 0xb5, 0x33, 0xf0,
                                                0x08, 0x32, 0xd5, 0x56, 0xb1, 0xf9, 0xea, 0xe5, 0x1d, 0x7d};
const h256 R1L{{&R1Array[0], h256::size()}};
const h160 R1S{{&R1Array[0], h160::size()}};

TEST_CASE("Hash", "[types]") {
    h256 hash;
    CHECK_FALSE(hash);  // Is empty

    // Empty hash hex
    std::string expected_hex{"0000000000000000000000000000000000000000000000000000000000000000"};
    std::string out_hex{hash.to_hex()};
    REQUIRE(out_hex == expected_hex);

    // Exact length valid hex
    std::string input_hex{"0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"};
    auto parsed_hash = h256::from_hex(input_hex);
    REQUIRE(parsed_hash);
    REQUIRE(parsed_hash->data()[0] == 0xc5);
    out_hex = parsed_hash->to_hex(/* with_prefix=*/true);
    REQUIRE(input_hex == out_hex);

    // Exact length invalid hex
    input_hex = "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85zzzz";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE((!parsed_hash && parsed_hash.error() == DecodingError::kInvalidHexDigit));

    // Oversize length (Hash loaded but null)
    input_hex = "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470000000";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE(parsed_hash);
    REQUIRE(!(*parsed_hash));  // Is empty

    // Shorter length valid hex
    input_hex = "0x460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470";
    parsed_hash = h256::from_hex(input_hex);
    REQUIRE(parsed_hash);
    out_hex = parsed_hash->to_hex(/*with_prefix=*/true);
    REQUIRE(input_hex != out_hex);
    expected_hex = "0x0000460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470";
    REQUIRE(out_hex == expected_hex);

    // Comparison
    auto parsed_hash1 = h256::from_hex("0x01");
    REQUIRE(parsed_hash1);
    auto parsed_hash2 = h256::from_hex("0x02");
    REQUIRE(parsed_hash2);
    REQUIRE((parsed_hash1 != parsed_hash2));
}

TEST_CASE("Hash to jenkins hash", "[types]") {
    auto salt{h256::from_hex("00112233445566778899aabbccddeeff00")};
    CHECK(salt);
    const size_t buffer_size{zen::h256::size() / sizeof(uint32_t)};
    std::array<uint32_t, buffer_size> saltbuf{0};
    std::memcpy(&saltbuf, salt->data(), zen::Hash<256>::size());

    std::array<uint32_t, buffer_size> r1lbuf{0};
    std::memcpy(&r1lbuf, R1L.data(), zen::h256::size());

    uint64_t hash1{R1L.hash(*salt)};
    uint64_t hash2{crypto::Jenkins::Hash(&r1lbuf[0], buffer_size, &saltbuf[0])};
    CHECK(hash1 == hash2);
}
}  // namespace zen
