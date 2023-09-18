/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <limits>
#include <random>
#include <string_view>

#include <boost/asio/ip/address.hpp>
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

//! \brief Parses a string representing an unsigned integer
template <typename T>
requires std::unsigned_integral<T>
bool try_parse_uint(std::string_view input, int base, T& output) noexcept {
    size_t pos{0};
    if (input.empty()) return false;
    auto input_str{std::string(input)};
    auto value{std::stoull(input_str, &pos, base)};
    if (pos not_eq input_str.length() or value > std::numeric_limits<T>::max()) {
        return false;
    }
    output = static_cast<T>(value);
    return true;
}

//! \brief Parses a string representing an IP address and port
//! \remarks If port is not provided then it will be set to zero
//! \details The following formats are supported:
//! - [ipv6_address]:port
//! - ipv4_address:port
//! - ipv4_address
//! - [ipv6_address]
bool try_parse_ip_address_and_port(std::string_view input, boost::asio::ip::address& address, uint16_t& port) noexcept;

//! \brief Generates a random value of type T in a provided (min, max) range
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize(const T min, const T max) {
    ZEN_THREAD_LOCAL std::random_device rnd;
    ZEN_THREAD_LOCAL std::mt19937 gen(rnd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

//! \brief Generates a random value of type T in range (min,std::numeric_limits<T>::max())
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize(const T min) {
    return randomize<T>(static_cast<T>(min), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range (std::numeric_limits<T>::max(),std::numeric_limits<T>::max())
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize() {
    return randomize<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range (T * (1.0F - percentage), T * (1.0F + percentage))
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type randomize(T val, float percentage) {
    percentage = std::abs(percentage);
    if (percentage > 1.0F) percentage = 1.0F;
    const T min = static_cast<T>(val * (1.0F - percentage));
    const T max = static_cast<T>(val * (1.0F + percentage));
    return randomize<T>(min, max);
}
}  // namespace zenpp
