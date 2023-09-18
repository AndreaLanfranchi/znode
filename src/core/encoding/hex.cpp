/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "hex.hpp"

#include <array>
#include <ranges>

namespace zenpp::hex {

// ASCII -> hex value (0xff means bad [hex] char)
static constexpr std::array<uint8_t, 256> kUnhexTable{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// ASCII -> hex value << 4 (upper nibble) (0xff means bad [hex] char)
static constexpr std::array<uint8_t, 256> kUnhexTable4{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
    0x90, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// clang-format on

std::string reverse_hex(std::string_view input) noexcept {
    if (input.empty()) return {};

    std::string output;
    output.reserve(input.length());
    if (has_prefix(input)) {
        output += "0x";
        input.remove_prefix(2);
    }

    while (input.size() >= 2) {
        output.append(input.substr(input.size() - 2));
        input.remove_suffix(2);
    }
    if (!input.empty()) [[unlikely]] {
        output.push_back('0');
        output.push_back(input.back());
    }

    return output;
}

ByteView zeroless_view(ByteView data) {
    const auto first_not_zero = std::ranges::find_if_not(data, [](const auto b) { return b == 0x0; });
    if (first_not_zero == data.end()) return {};  // An empty string
    const auto substr_offset{static_cast<size_t>(std::distance(data.begin(), first_not_zero))};
    data.remove_prefix(substr_offset);
    return data;
}

std::string encode(ByteView bytes, bool with_prefix) noexcept {
    static const std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                                 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string out(bytes.length() * 2 + (with_prefix ? 2 : 0), 0x0);
    auto* dest{out.data()};
    const auto* src{bytes.data()};
    if (with_prefix) {
        *dest++ = '0';
        *dest++ = 'x';
    }
    for (ByteView::size_type i{0}; i < bytes.length(); ++i, ++src) {
        *dest++ = kHexDigits[*src >> 4];
        *dest++ = kHexDigits[*src & 0x0f];
    }
    return out;
}

tl::expected<Bytes, DecodingError> decode(std::string_view hex_str) noexcept {
    if (has_prefix(hex_str)) {
        hex_str.remove_prefix(2);
    }
    if (hex_str.empty()) {
        return Bytes{};
    }

    const size_t pos(hex_str.length() & 1);  // "[0x]1" is legit and has to be treated as "[0x]01"
    Bytes out((hex_str.length() + pos) / 2, '\0');
    auto* dst{out.data()};
    const auto* src{hex_str.data()};
    const auto* last = src + hex_str.length();

    if (pos not_eq 0U) {
        const auto b{kUnhexTable[static_cast<uint8_t>(*src++)]};
        if (b == 0xff) {
            return tl::unexpected{DecodingError::kInvalidHexDigit};
        }
        *dst++ = b;
    }

    // following "while" is unrolling the loop when we have >= 4 target bytes
    // this is optional, but 5-10% faster
    while (last - src >= 8) {
        const auto a{kUnhexTable4[static_cast<uint8_t>(*src++)]};
        const auto b{kUnhexTable[static_cast<uint8_t>(*src++)]};
        const auto c{kUnhexTable4[static_cast<uint8_t>(*src++)]};
        const auto d{kUnhexTable[static_cast<uint8_t>(*src++)]};
        const auto e{kUnhexTable4[static_cast<uint8_t>(*src++)]};
        const auto f{kUnhexTable[static_cast<uint8_t>(*src++)]};
        const auto g{kUnhexTable4[static_cast<uint8_t>(*src++)]};
        const auto h{kUnhexTable[static_cast<uint8_t>(*src++)]};
        if ((b | d | f | h) == 0xff || (a | c | e | g) == 0xff) {
            return tl::unexpected{DecodingError::kInvalidHexDigit};
        }
        *dst++ = static_cast<uint8_t>(a | b);
        *dst++ = static_cast<uint8_t>(c | d);
        *dst++ = static_cast<uint8_t>(e | f);
        *dst++ = static_cast<uint8_t>(g | h);
    }

    while (src < last) {
        const auto a{kUnhexTable4[static_cast<uint8_t>(*src++)]};
        const auto b{kUnhexTable[static_cast<uint8_t>(*src++)]};
        if (a == 0xff || b == 0xff) {
            return tl::unexpected{DecodingError::kInvalidHexDigit};
        }
        *dst++ = static_cast<uint8_t>(a | b);
    }
    return out;
}

tl::expected<unsigned, DecodingError> decode_digit(const char input) noexcept {
    auto value = kUnhexTable[static_cast<uint8_t>(input)];
    if (value == 0xff) return tl::unexpected{DecodingError::kInvalidHexDigit};
    return value;
}
}  // namespace zenpp::hex
