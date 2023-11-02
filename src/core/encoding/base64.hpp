/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <core/common/base.hpp>
#include <core/encoding/errors.hpp>

namespace znode::enc::base64 {

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] outcome::result<std::string> encode(ByteView bytes) noexcept;

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] outcome::result<std::string> encode(std::string_view data) noexcept;

//! \brief Returns a string of bytes with the decoded base64 payload
//! \remark If provided an empty input the returned bytes are empty as well
[[nodiscard]] outcome::result<Bytes> decode(std::string_view input) noexcept;

}  // namespace znode::enc::base64
