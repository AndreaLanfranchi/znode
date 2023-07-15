/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <bit>
#include <concepts>
#include <ranges>

#include <zen/core/common/base.hpp>

namespace zen::endian {

// TODO(C++23) replace with std::byteswap from <bits> header
//! \brief Reverses the bytes for given integer value
template <std::integral T>
constexpr T byte_swap(T value) {
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    Bytes buffer(sizeof(T), 0);
    std::memcpy(buffer.data(), &value, sizeof(T));
    std::ranges::reverse(buffer);
    std::memcpy(&value, buffer.data(), sizeof(T));
    return value;
}

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

}  // namespace zen::endian
