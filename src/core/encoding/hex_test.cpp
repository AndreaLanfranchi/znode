/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <optional>
#include <vector>

#include <catch2/catch.hpp>

#include <core/encoding/hex.hpp>

namespace znode::enc::hex {
namespace {
    struct TestCaseDecodeHex {
        std::string hexstring;
        std::optional<enc::Error> expected;
        Bytes bytes;
    };

    const std::vector<TestCaseDecodeHex> TestCasesDecodeHex{
        {"0x", std::nullopt, {}},
        {"0xg", enc::Error::kIllegalHexDigit, {}},
        {"0", std::nullopt, {0x0}},
        {"0x0", std::nullopt, {0x0}},
        {"0xa", std::nullopt, {0x0a}},
        {"0xa1f", std::nullopt, {0x0a, 0x1f}},
        {"0x0a1f", std::nullopt, {0x0a, 0x1f}},
        {"111111111111111111111111",
         std::nullopt,
         {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}}};
}  // namespace

TEST_CASE("Decoding Hex", "[encoding][hex]") {
    CHECK_FALSE(decode_digit('0').has_error());
    CHECK_FALSE(decode_digit('1').has_error());
    CHECK_FALSE(decode_digit('5').has_error());
    CHECK_FALSE(decode_digit('a').has_error());
    CHECK_FALSE(decode_digit('d').has_error());
    CHECK_FALSE(decode_digit('f').has_error());
    CHECK_FALSE(decode_digit('g'));

    for (const auto& test : TestCasesDecodeHex) {
        const auto parsed_bytes{decode(test.hexstring)};
        if (test.expected.has_value()) {
            REQUIRE(parsed_bytes.has_error());
            REQUIRE(parsed_bytes.error().value() == static_cast<int>(test.expected.value()));
        } else {
            REQUIRE_FALSE(parsed_bytes.has_error());
            REQUIRE(parsed_bytes.value() == test.bytes);
        }
    }
}

TEST_CASE("Hex encoding integrals", "[encoding]][hex]") {
    uint32_t value{0};
    std::string expected_hex = "0x00";
    std::string obtained_hex = encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "00";
    obtained_hex = encode(value, false);
    CHECK(expected_hex == obtained_hex);

    value = 10;
    expected_hex = "0x0a";
    obtained_hex = encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = encode(value, false);
    CHECK(expected_hex == obtained_hex);

    value = 255;
    expected_hex = "0xff";
    obtained_hex = encode(value, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "ff";
    obtained_hex = encode(value, false);
    CHECK(expected_hex == obtained_hex);

    uint8_t value1{10};
    expected_hex = "0x0a";
    obtained_hex = encode(value1, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = encode(value1, false);
    CHECK(expected_hex == obtained_hex);

    uint64_t value2{10};
    expected_hex = "0x0a";
    obtained_hex = encode(value2, true);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0a";
    obtained_hex = encode(value2, false);
    CHECK(expected_hex == obtained_hex);
}

TEST_CASE("Hex encoding big integrals", "[encoding]][hex]") {
    uint256_t value("1182508626613988427021106");
    std::string expected_hex = "fa67ebcd123450abbb32";
    std::string obtained_hex = encode(value, false);
    CHECK(expected_hex == obtained_hex);
    expected_hex = "0xfa67ebcd123450abbb32";
    obtained_hex = encode(value, true);
    CHECK(expected_hex == obtained_hex);

    auto decoded_bytes{decode(expected_hex)};
    REQUIRE(decoded_bytes);
    uint256_t decoded_value{decoded_bytes.value()};
    CHECK(decoded_value == value);
}
}  // namespace znode::enc::hex
