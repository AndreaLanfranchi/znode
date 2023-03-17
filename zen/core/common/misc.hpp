/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <thread>

#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/encoding/errors.hpp>

namespace zen {

//! \brief Abridges a string to given length and eventually adds an ellipsis if input length is gt required length
//! \remarks Should length be equal to zero then no abridging occurs
[[nodiscard]] std::string abridge(std::string_view input, size_t length);

//! \brief Parses a string input value representing a size in human-readable format with qualifiers. eg "256MB"
[[nodiscard]] tl::expected<uint64_t, DecodingError> parse_human_bytes(const std::string& input);

//! \brief Transforms a size value into it's decimal string representation with suffix (optional binary)
//! \see https://en.wikipedia.org/wiki/Binary_prefix
[[nodiscard]] std::string to_human_bytes(size_t input, bool binary = false);

//! \brief Builds a randomized string of alpha num chars (lowercase) of arbitrary length
[[nodiscard]] std::string get_random_alpha_string(size_t length);

}  // namespace zen
