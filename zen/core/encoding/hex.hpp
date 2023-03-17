/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <string_view>

#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/encoding/errors.hpp>

namespace zen::hex {

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

//! \brief Returns a string of ascii chars with the hexadecimal representation of provided unsigned integral
template <UnsignedIntegralEx T>
[[nodiscard]] std::string encode(const T value, bool with_prefix = false) noexcept {
    uint8_t bytes[sizeof(T)];
    intx::be::store(bytes, value);
    std::string hexed{encode(zeroless_view(bytes), with_prefix)};
    if (hexed.length() == (with_prefix ? 2 : 0)) {
        hexed += "00";
    }
    return hexed;
}

//! \brief Returns the bytes string obtained by decoding an hexadecimal ascii input
// TODO(C++23) switch to std::expected
tl::expected<Bytes, DecodingError> decode(std::string_view source) noexcept;

//! \brief Returns the integer value corresponding to the ascii hex digit provided
tl::expected<unsigned, DecodingError> decode_digit(char input) noexcept;

}  // namespace zen::hex
