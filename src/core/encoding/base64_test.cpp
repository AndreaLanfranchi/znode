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

#include <vector>

#include <catch2/catch.hpp>

#include <core/common/cast.hpp>
#include <core/encoding/base64.hpp>

namespace znode::enc::base64 {

namespace {
    // See https://www.rfc-editor.org/rfc/rfc4648#section-10
    const std::vector<std::pair<std::string, std::string>> test_cases{
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
    };

}  // namespace

TEST_CASE("Base64 encoding", "[encoding]") {
    for (const auto& [input, expected_output] : test_cases) {
        const auto encoded_output{encode(input)};
        REQUIRE(encoded_output);
        CHECK(encoded_output.value() == expected_output);
    }
}

TEST_CASE("Base64 decoding", "[encoding]") {
    for (const auto& [expected_output, input] : test_cases) {
        const auto decoded_output{decode(input)};
        REQUIRE(decoded_output);
        CHECK(byte_view_to_string_view(decoded_output.value()) == expected_output);
    }

    // Decode input with invalid char
    std::string invalid_input{"Zg&aa"};
    const auto decoded_output{decode(invalid_input)};
    REQUIRE_FALSE(decoded_output);
}
}  // namespace znode::enc::base64
