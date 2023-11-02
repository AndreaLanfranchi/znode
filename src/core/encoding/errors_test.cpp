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

#include <core/common/base.hpp>
#include <core/encoding/errors.hpp>

namespace znode::enc {
namespace {
    std::string to_string(enc::Error error) { return std::string(magic_enum::enum_name<Error>(error)).erase(0, 1); }

    outcome::result<void> test_result(int code) {
        const auto enumerator = magic_enum::enum_cast<enc::Error>(code);
        if (not enumerator.has_value()) {
            return outcome::failure(boost::system::error_code{code, enc::ErrorCategory()});
        }
        return *enumerator;
    }

}  // namespace
TEST_CASE("Encoding errors", "[enc][errors]") {
    CHECK(to_string(enc::Error::kSuccess) == "Success");
    CHECK(to_string(enc::Error::kIllegalHexDigit) == "IllegalHexDigit");
    CHECK(to_string(enc::Error::kIllegalBase58Digit) == "IllegalBase58Digit");
    CHECK(to_string(enc::Error::kIllegalBase64Digit) == "IllegalBase64Digit");
    CHECK(to_string(enc::Error::kInputTooLarge) == "InputTooLarge");
    CHECK(to_string(enc::Error::kInputTooNarrow) == "InputTooNarrow");
    CHECK(to_string(enc::Error::kUnexpectedError) == "UnexpectedError");

    for (auto enumerator : magic_enum::enum_values<enc::Error>()) {
        auto code = static_cast<int>(enumerator);
        auto label = to_string(enumerator);
        if (code == 0) continue;

        INFO("Testing error code [" << code << ":" << label << "]");
        CHECK(enc::ErrorCategory().message(static_cast<int>(enumerator)) == label);

        const auto result = test_result(code);
        REQUIRE(result.has_error());
        REQUIRE(result.error().value() == code);
        REQUIRE(result.error().message() == label);
    }
}
}  // namespace znode::enc