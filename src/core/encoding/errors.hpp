/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <string>

#include <boost/system/error_code.hpp>
#include <magic_enum.hpp>

namespace zenpp::enc {

enum class Error {
    kSuccess,             // Not actually an error
    kIllegalHexDigit,     // One or more input characters is not a valid hex digit
    kIllegalBase58Digit,  // One or more input characters is not a valid base58 digit
    kIllegalBase64Digit,  // One or more input characters is not a valid base64 digit
    kInputTooLarge,       // Input is too large for function to handle
    kInputTooNarrow,      // Function expects an input of a certain size or more
    kUnexpectedError,     // An unexpected error occurred
};

class ErrorCategory final : public boost::system::error_category {
  public:
    virtual ~ErrorCategory() noexcept = default;
    const char* name() const noexcept override { return "EncodingError"; }
    std::string message(int err_code) const override {
        std::string desc{"Unknown error"};
        if (const auto enumerator = magic_enum::enum_cast<enc::Error>(err_code); enumerator.has_value()) {
            desc.assign(std::string(magic_enum::enum_name<enc::Error>(*enumerator)));
            desc.erase(0, 1);  // Remove the constant `k` prefix
        }
        return desc;
    }
    boost::system::error_condition default_error_condition(int err_code) const noexcept override {
        const auto enumerator = magic_enum::enum_cast<enc::Error>(err_code);
        if (not enumerator.has_value()) {
            return {err_code, *this};  // No conversion
        }
        switch (*enumerator) {
            using enum Error;
            case kSuccess:
                return make_error_condition(boost::system::errc::success);
            case kIllegalHexDigit:
            case kIllegalBase58Digit:
            case kIllegalBase64Digit:
                return make_error_condition(boost::system::errc::argument_out_of_domain);
            case kInputTooLarge:
                return make_error_condition(boost::system::errc::value_too_large);
            case kInputTooNarrow:
                return make_error_condition(boost::system::errc::invalid_argument);
            case kUnexpectedError:
                return make_error_condition(boost::system::errc::io_error);
            default:
                return {err_code, *this};
        }
    }
};

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline boost::system::error_code make_error_code(enc::Error err_code) {
    static enc::ErrorCategory category{};
    return {static_cast<int>(err_code), category};
}
}  // namespace zenpp::enc

namespace boost::system {
// Tell the C++ 11 STL metaprogramming that our enums are registered with the
// error code system
template <>
struct is_error_code_enum<zenpp::enc::Error> : public std::true_type {};
}  // namespace boost::system
