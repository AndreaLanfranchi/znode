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

namespace zenpp::ser {

enum class Error {
    kSuccess,                  // Not actually an error
    kInputTooLarge,            // The input is too large
    kReadOverflow,             // The read operation would overflow the buffer
    kNonCanonicalCompactSize,  // The compact size is not canonical
    kCompactSizeTooBig,        // The compact size is too big
    kUnexpectedError,          // An unexpected error occurred
};

class ErrorCategory : public boost::system::error_category {
  public:
    virtual ~ErrorCategory() noexcept = default;
    const char* name() const noexcept override { return "SerializationError"; }
    std::string message(int err_code) const override {
        std::string desc{"Unknown error"};
        if (const auto enumerator = magic_enum::enum_cast<ser::Error>(err_code); enumerator.has_value()) {
            desc.assign(std::string(magic_enum::enum_name<ser::Error>(*enumerator)));
            desc.erase(0, 1);  // Remove the constant `k` prefix
        }
        return desc;
    }
    boost::system::error_condition default_error_condition(int err_code) const noexcept override {
        const auto enumerator = magic_enum::enum_cast<ser::Error>(err_code);
        if (not enumerator.has_value()) {
            return {err_code, *this};  // No conversion
        }
        switch (*enumerator) {
            using enum Error;
            case kSuccess:
                return make_error_condition(boost::system::errc::success);
            case kInputTooLarge:
            case kReadOverflow:
                return make_error_condition(boost::system::errc::value_too_large);
            case kNonCanonicalCompactSize:
            case kCompactSizeTooBig:
                return make_error_condition(boost::system::errc::argument_out_of_domain);
            case kUnexpectedError:
                return make_error_condition(boost::system::errc::io_error);
            default:
                return {err_code, *this};
        }
    }
};

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline boost::system::error_code make_error_code(ser::Error err_code) {
    static ser::ErrorCategory category{};
    return {static_cast<int>(err_code), category};
}
}  // namespace zenpp::ser

namespace boost::system {
// Tell the C++ 11 STL metaprogramming that our enums are registered with the
// error code system
template <>
struct is_error_code_enum<zenpp::ser::Error> : public std::true_type {};
}  // namespace boost::system
