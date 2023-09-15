/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>
#include <bit>
#include <concepts>
#include <ranges>

#include <core/common/base.hpp>

#if __cplusplus < 202300L
namespace std {
//! \brief Reverses the order of bytes in the object representation of value.
//! \remarks This function is available in std C++23. This is a backport
//! \tparam T The type of the object whose bytes must be reversed.
//! \param value The input value.
//! \return The byte reversed representation of value.
//! \see https://en.cppreference.com/w/cpp/numeric/byteswap
template <std::integral T>
constexpr T byteswap(T value) noexcept {
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    std::array<std::byte, sizeof(T)> value_bytes{std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
    std::ranges::reverse(value_bytes);
    return std::bit_cast<T>(value_bytes);
}
}  // namespace std
#endif

namespace zenpp::endian {

// Similar to boost::endian::load_big_u16
const auto load_big_u16 = intx::be::unsafe::load<uint16_t>;

// Similar to boost::endian::load_big_u32
const auto load_big_u32 = intx::be::unsafe::load<uint32_t>;

// Similar to boost::endian::load_big_u64
const auto load_big_u64 = intx::be::unsafe::load<uint64_t>;

// Similar to boost::endian::load_little_u16
const auto load_little_u16 = intx::le::unsafe::load<uint16_t>;

// Similar to boost::endian::load_little_u32
const auto load_little_u32 = intx::le::unsafe::load<uint32_t>;

// Similar to boost::endian::load_little_u64
const auto load_little_u64 = intx::le::unsafe::load<uint64_t>;

// Similar to boost::endian::store_big_u16
const auto store_big_u16 = intx::be::unsafe::store<uint16_t>;

// Similar to boost::endian::store_big_u32
const auto store_big_u32 = intx::be::unsafe::store<uint32_t>;

// Similar to boost::endian::store_big_u64
const auto store_big_u64 = intx::be::unsafe::store<uint64_t>;

// Similar to boost::endian::store_little_u16
const auto store_little_u16 = intx::le::unsafe::store<uint16_t>;

// Similar to boost::endian::store_little_u32
const auto store_little_u32 = intx::le::unsafe::store<uint32_t>;

// Similar to boost::endian::store_little_u64
const auto store_little_u64 = intx::le::unsafe::store<uint64_t>;

}  // namespace zenpp::endian
