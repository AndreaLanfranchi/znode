/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <exception>
#include <string>

#include <magic_enum.hpp>

#include <core/serialization/base.hpp>

namespace zen::serialization {

class SerializationException : public std::exception {
  public:
    explicit SerializationException(const char* message) : message_(message) {}
    explicit SerializationException(const char* message, const Error err) : message_(message), error_(err) {}
    explicit SerializationException(const Error err) : message_(std::string(magic_enum::enum_name(err))), error_(err) {}
    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
    [[nodiscard]] Error error() const noexcept { return error_; }
    [[nodiscard]] uint32_t error_code() const noexcept { return static_cast<uint32_t>(error_); }

  private:
    std::string message_;
    serialization::Error error_{Error::kUndefinedError};
};

inline void success_or_throw(Error err) {
    if (err == Error::kSuccess) return;
    throw SerializationException(err);
}
}  // namespace zen::serialization
