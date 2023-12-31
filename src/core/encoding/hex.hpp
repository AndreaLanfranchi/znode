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

#include <string_view>

#include <boost/endian/conversion.hpp>

#include <core/common/base.hpp>
#include <core/encoding/errors.hpp>

namespace znode::enc::hex {

//! \brief Builds a randomized hex string of arbitrary length
[[nodiscard]] std::string get_random(size_t length);

//! \brief Whether provided string begins with "0x" prefix (case insensitive)
//! \param [in] source : string input
//! \return true/false
inline bool has_prefix(std::string_view source) {
    return source.length() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X');
}

//! \brief Reverses an hex string
[[nodiscard]] std::string reverse_hex(std::string_view input) noexcept;

//! \brief Strips leftmost zeroed bytes from byte sequence
//! \param [in] data : The view to process
//! \return A new view of the sequence
ByteView zeroless_view(ByteView data);

//! \brief Returns a string of ascii chars with the hexadecimal representation of input
//! \remark If provided an empty input the return string is empty as well (with prefix if requested)
[[nodiscard]] std::string encode(ByteView bytes, bool with_prefix = false) noexcept;

//! \brief Returns a string of ascii chars with the hexadecimal representation of provided unsigned big integral
template <BigUnsignedIntegral T>
[[nodiscard]] std::string encode(const T value, bool with_prefix = false) noexcept {
    Bytes bytes{};
    boost::multiprecision::export_bits(value, std::back_inserter(bytes), CHAR_BIT, true);
    auto hexed{encode(zeroless_view(bytes), with_prefix)};
    if (hexed.length() == (with_prefix ? 2U : 0U)) {
        hexed += "00";
    }
    return hexed;
}

//! \brief Returns a string of ascii chars with the hexadecimal representation of provided unsigned integral
template <UnsignedIntegral T>
[[nodiscard]] std::string encode(const T value, bool with_prefix = false) noexcept {
    Bytes bytes(sizeof(T), 0x0);
    std::memcpy(bytes.data(), &value, sizeof(T));
    if (bytes.size() not_eq 1U) {
        std::ranges::reverse(bytes);
    }
    auto hexed{encode(zeroless_view(bytes), with_prefix)};
    if (hexed.length() == (with_prefix ? 2U : 0U)) {
        hexed += "00";
    }
    return hexed;
}

//! \brief Returns the bytes string obtained by decoding an hexadecimal ascii input
outcome::result<Bytes> decode(std::string_view hex_str) noexcept;

//! \brief Returns the integer value corresponding to the ascii hex digit provided
outcome::result<unsigned> decode_digit(char input) noexcept;

}  // namespace znode::enc::hex
