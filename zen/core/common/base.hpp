/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <intx/intx.hpp>

#if defined(__wasm__)
#define ZEN_THREAD_LOCAL
#else
#define ZEN_THREAD_LOCAL thread_local
#endif

#if defined(BOOST_NO_EXCEPTIONS)
#include <boost/throw_exception.hpp>
void boost::throw_exception(const std::exception& ex);
#endif

namespace zen {

using BlockNum = uint32_t;

template <class T>
concept UnsignedIntegral = std::unsigned_integral<T>;

template <class T>
concept UnsignedIntegralEx = UnsignedIntegral<T> || std::same_as<T, intx::uint128> || std::same_as<T, intx::uint256> ||
    std::same_as<T, intx::uint512>;

using Bytes = std::basic_string<uint8_t>;

class ByteView : public std::basic_string_view<uint8_t> {
  public:
    constexpr ByteView() noexcept = default;

    constexpr ByteView(const std::basic_string_view<uint8_t>& other) noexcept
        : std::basic_string_view<uint8_t>{other.data(), other.length()} {}

    constexpr ByteView(const Bytes& str) noexcept : std::basic_string_view<uint8_t>{str.data(), str.length()} {}

    constexpr ByteView(const uint8_t* data, size_type length) noexcept
        : std::basic_string_view<uint8_t>{data, length} {}

    template <std::size_t N>
    constexpr ByteView(const uint8_t (&array)[N]) noexcept : std::basic_string_view<uint8_t>{array, N} {}

    template <std::size_t N>
    constexpr ByteView(const std::array<uint8_t, N>& array) noexcept
        : std::basic_string_view<uint8_t>{array.data(), N} {}

    [[nodiscard]] bool is_null() const noexcept { return data() == nullptr; }
};

// Sizes base 10
inline constexpr uint64_t kKB{1'000};        // 10^{3} bytes
inline constexpr uint64_t kMB{kKB * 1'000};  // 10^{6} bytes
inline constexpr uint64_t kGB{kMB * 1'000};  // 10^{9} bytes
inline constexpr uint64_t kTB{kGB * 1'000};  // 10^{12} bytes

// Sizes base 2 https://en.wikipedia.org/wiki/Binary_prefix
inline constexpr uint64_t kKiB{1024};        // 2^{10} bytes
inline constexpr uint64_t kMiB{kKiB << 10};  // 2^{20} bytes
inline constexpr uint64_t kGiB{kMiB << 10};  // 2^{30} bytes
inline constexpr uint64_t kTiB{kGiB << 10};  // 2^{40} bytes

// Literals for sizes base 10
constexpr uint64_t operator"" _KB(unsigned long long x) { return x * kKB; }
constexpr uint64_t operator"" _MB(unsigned long long x) { return x * kMB; }
constexpr uint64_t operator"" _GB(unsigned long long x) { return x * kGB; }
constexpr uint64_t operator"" _TB(unsigned long long x) { return x * kTB; }

// Literals for sizes base 2
constexpr uint64_t operator"" _KiB(unsigned long long x) { return x * kKiB; }
constexpr uint64_t operator"" _MiB(unsigned long long x) { return x * kMiB; }
constexpr uint64_t operator"" _GiB(unsigned long long x) { return x * kGiB; }
constexpr uint64_t operator"" _TiB(unsigned long long x) { return x * kTiB; }

static constexpr int64_t kCoinMaxDecimals = 8;         // Max number of denomination decimals
static constexpr int64_t kCoin = 100'000'000;          // As many zeroes as kCoinMaxDecimals
static constexpr int64_t kCoinCent = kCoin / 100;      // One coin cent
static constexpr int64_t kCoinMaxSupply = 21'000'000;  // Max tokens supply
static constexpr std::string_view kCurrency{"ZEN"};

}  // namespace zen
