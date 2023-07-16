/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <catch2/catch.hpp>

#include <core/encoding/hex.hpp>

namespace zenpp::hex {

struct TestCaseDecodeHex {
    std::string hexstring;
    DecodingError expected;
    Bytes bytes;
};

static const std::vector<TestCaseDecodeHex> TestCasesDecodeHex{
    {"0x", DecodingError::kSuccess, {}},
    {"0xg", DecodingError::kInvalidHexDigit, {}},
    {"0", DecodingError::kSuccess, {0x0}},
    {"0x0", DecodingError::kSuccess, {0x0}},
    {"0xa", DecodingError::kSuccess, {0x0a}},
    {"0xa1f", DecodingError::kSuccess, {0x0a, 0x1f}},
    {"0x0a1f", DecodingError::kSuccess, {0x0a, 0x1f}},
    {"111111111111111111111111",
     DecodingError::kSuccess,
     {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}}};

TEST_CASE("Decoding Hex", "[encoding]") {
    CHECK(hex::decode_digit('0'));
    CHECK(hex::decode_digit('1'));
    CHECK(hex::decode_digit('5'));
    CHECK(hex::decode_digit('a'));
    CHECK(hex::decode_digit('d'));
    CHECK(hex::decode_digit('f'));
    CHECK_FALSE(hex::decode_digit('g'));

    for (const auto& test : TestCasesDecodeHex) {
        auto parsed_bytes{hex::decode(test.hexstring)};
        if (test.expected != DecodingError::kSuccess) {
            REQUIRE_FALSE(parsed_bytes);
            REQUIRE(parsed_bytes.error() == test.expected);
        } else {
            REQUIRE(parsed_bytes);
            REQUIRE(*parsed_bytes == test.bytes);
        }
    }
}

TEST_CASE("Hex encoding integrals", "[encoding]") {
    uint32_t value{0};
    std::string expected_hex = "0x00";
    std::string obtained_hex = hex::encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "00";
    obtained_hex = hex::encode(value, false);
    CHECK(expected_hex == obtained_hex);

    value = 10;
    expected_hex = "0x0a";
    obtained_hex = hex::encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = hex::encode(value, false);
    CHECK(expected_hex == obtained_hex);

    value = 255;
    expected_hex = "0xff";
    obtained_hex = hex::encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "ff";
    obtained_hex = hex::encode(value, false);
    CHECK(expected_hex == obtained_hex);

    uint8_t value1{10};
    expected_hex = "0x0a";
    obtained_hex = hex::encode(value1, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = hex::encode(value1, false);
    CHECK(expected_hex == obtained_hex);

    uint64_t value2{10};
    expected_hex = "0x0a";
    obtained_hex = hex::encode(value2, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = hex::encode(value2, false);
    CHECK(expected_hex == obtained_hex);
}

}  // namespace zenpp::hex
