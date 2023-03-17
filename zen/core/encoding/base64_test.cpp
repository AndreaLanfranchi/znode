/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/common/cast.hpp>
#include <zen/core/encoding/base64.hpp>

namespace zen::base64 {

TEST_CASE("Base64 encoding", "[encoding]") {
    // See https://www.rfc-editor.org/rfc/rfc4648#section-10
    const std::vector<std::pair<std::string, std::string>> tests{
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
    };

    for (const auto& [input, expected_output] : tests) {
        const auto encoded_output{encode(input)};
        CHECK(encoded_output == expected_output);
    }
}

TEST_CASE("Base64 decoding", "[encoding]") {
    // See https://www.rfc-editor.org/rfc/rfc4648#section-10
    const std::vector<std::pair<std::string, std::string>> tests{
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
    };

    for (const auto& [expected_output, input] : tests) {
        const auto decoded_output{decode(input)};
        REQUIRE(decoded_output);
        CHECK(byte_view_to_string_view(*decoded_output) == expected_output);
    }

    // Decode input with invalid char
    std::string invalid_input{"Zg&aa"};
    const auto decoded_output{decode(invalid_input)};
    REQUIRE_FALSE(decoded_output);
}
}  // namespace zen::base64
