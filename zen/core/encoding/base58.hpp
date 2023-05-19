/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/encoding/errors.hpp>

namespace zen::base58 {

//! \brief Returns a string of ascii chars with the base58 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] tl::expected<std::string, EncodingError> encode(ByteView bytes) noexcept;

}  // namespace zen::base58
