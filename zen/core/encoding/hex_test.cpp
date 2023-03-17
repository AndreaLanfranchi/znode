/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/encoding/hex.hpp>

namespace zen {

TEST_CASE("Decoding Hex", "[encoding]") {
    CHECK(hex::decode_digit('0'));
    CHECK(hex::decode_digit('1'));
    CHECK(hex::decode_digit('5'));
    CHECK(hex::decode_digit('a'));
    CHECK(hex::decode_digit('d'));
    CHECK(hex::decode_digit('f'));
    CHECK_FALSE(hex::decode_digit('g'));

    auto parsed_bytes{hex::decode("")};
    CHECK((parsed_bytes && parsed_bytes->empty()));

    parsed_bytes = hex::decode("0x");
    CHECK((parsed_bytes && parsed_bytes->empty()));

    parsed_bytes = hex::decode("0xg");
    CHECK_FALSE(parsed_bytes);
    CHECK(parsed_bytes.error() == DecodingError::kInvalidHexDigit);

    Bytes expected_bytes{0x0};
    parsed_bytes = hex::decode("0");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));
    parsed_bytes = hex::decode("0x0");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));

    expected_bytes = {0x0a};
    parsed_bytes = hex::decode("0xa");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));
    parsed_bytes = hex::decode("0x0a");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));

    expected_bytes = {0x0a, 0x1f};
    parsed_bytes = hex::decode("0xa1f");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));
    parsed_bytes = hex::decode("0x0a1f");
    CHECK((parsed_bytes && *parsed_bytes == expected_bytes));

    std::string source(24, '1');
    expected_bytes = Bytes(12, 0x11);
    for (char& c : source) {
        parsed_bytes = hex::decode(source);
        CHECK(parsed_bytes);
        c = 'k';
        parsed_bytes = hex::decode(source);
        CHECK_FALSE(parsed_bytes);
        c = '1';
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

}  // namespace zen
