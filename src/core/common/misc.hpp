/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <limits>
#include <random>

#include <tl/expected.hpp>

#include <core/common/base.hpp>
#include <core/encoding/errors.hpp>

namespace zenpp {

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

//! \brief Provided a view of data returns the number of duplicate chunks of given size
//! \remarks If max_count is set to zero then the function will return the total number of duplicate chunks found
//! otherwise it will stop counting and return as soon as max_count is reached
[[nodiscard]] size_t count_duplicate_data_chunks(ByteView data, size_t chunk_size, size_t max_count = 0) noexcept;

//! \brief Generates a random value of type T in range std::numeric_limits<T>::max() to std::numeric_limits<T>::max()
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<T> dis(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    return dis(gen);
}

//! \brief Generates a random value of type T in a provided (min, max) range
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize(T min, T max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}
}  // namespace zenpp
