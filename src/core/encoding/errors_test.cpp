/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/common/base.hpp>
#include <core/encoding/errors.hpp>

namespace zenpp::enc {
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
}  // namespace zenpp::enc