/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <tl/expected.hpp>

#include <core/common/base.hpp>
#include <core/crypto/md.hpp>
#include <core/encoding/errors.hpp>

namespace zenpp::base58 {

//! \brief Returns a string of ascii chars with the base58 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<std::string, EncodingError> encode(ByteView input) noexcept;

//! \brief Returns a string of ascii chars with the base58 representation of input added of 4 bytes from its Digest256
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<std::string, EncodingError> encode_check(ByteView input) noexcept;

//! \brief Returns a string of bytes with the decoded base58 payload
//! \remark If provided an empty input the returned bytes are empty as well
[[nodiscard]] tl::expected<Bytes, DecodingError> decode(std::string_view input) noexcept;

//! \brief Returns a string of bytes with the decoded base58 representation of input added of 4 bytes from its Digest256
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<Bytes, DecodingError> decode_check(std::string_view input) noexcept;

}  // namespace zenpp::base58
