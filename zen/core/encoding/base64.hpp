/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/encoding/errors.hpp>

namespace zen::base64 {

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<std::string, EncodingError> encode(ByteView bytes) noexcept;

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<std::string, EncodingError> encode(std::string_view data) noexcept;

//! \brief Returns a string of ascii chars with the decoded base64 payload
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<Bytes, DecodingError> decode(std::string_view input) noexcept;

}  // namespace zen::base64
