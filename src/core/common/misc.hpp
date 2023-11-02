/*
   Copyright 2022 The Silkworm Authors
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

#pragma once

#include <limits>
#include <random>
#include <string_view>

#include <boost/asio/ip/address.hpp>

#include <core/common/base.hpp>

namespace znode {

//! \brief Abridges a string to given length and eventually adds an ellipsis if input length is gt required length
//! \remarks Should length be equal to zero then no abridging occurs
[[nodiscard]] std::string abridge(std::string_view input, size_t length);

//! \brief Parses a string input value representing a size in human-readable format with qualifiers. eg "256MB"
[[nodiscard]] outcome::result<uint64_t> parse_human_bytes(const std::string& input);

//! \brief Transforms a size value into it's decimal string representation with suffix (optional binary)
//! \see https://en.wikipedia.org/wiki/Binary_prefix
[[nodiscard]] std::string to_human_bytes(size_t input, bool binary = false);

//! \brief Builds a randomized string of alpha num chars (lowercase) of arbitrary length
[[nodiscard]] std::string get_random_alpha_string(size_t length);

//! \brief Provided a view of data returns the number of duplicate chunks of given size
//! \remarks If max_count is set to zero then the function will return the total number of duplicate chunks found
//! otherwise it will stop counting and return as soon as max_count is reached
[[nodiscard]] size_t count_duplicate_data_chunks(ByteView data, size_t chunk_size, size_t max_count = 0) noexcept;

}  // namespace znode
