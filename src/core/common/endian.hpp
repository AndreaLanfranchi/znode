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
#include <array>
#include <bit>
#include <ranges>

static_assert(std::endian::native == std::endian::little, "Target architecture MUST be little endian");

#include <boost/endian/conversion.hpp>

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
#endif  // __cplusplus < 202300L

namespace znode::endian {

const auto load_big_u16 = boost::endian::load_big_u16;
const auto load_big_u32 = boost::endian::load_big_u32;
const auto load_big_u64 = boost::endian::load_big_u64;

const auto load_little_u16 = boost::endian::load_little_u16;
const auto load_little_u32 = boost::endian::load_little_u32;
const auto load_little_u64 = boost::endian::load_little_u64;

const auto store_big_u16 = boost::endian::store_big_u16;
const auto store_big_u32 = boost::endian::store_big_u32;
const auto store_big_u64 = boost::endian::store_big_u64;

const auto store_little_u16 = boost::endian::store_little_u16;
const auto store_little_u32 = boost::endian::store_little_u32;
const auto store_little_u64 = boost::endian::store_little_u64;

}  // namespace znode::endian
