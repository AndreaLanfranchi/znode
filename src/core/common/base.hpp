/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

// clang-format off
#include <core/common/preprocessor.hpp>  // Must be first
// clang-format on

#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <boost/multiprecision/cpp_int.hpp>
#include <zenpp/buildinfo.h>

#if defined(BOOST_NO_EXCEPTIONS)
#include <boost/throw_exception.hpp>
namespace boost {
BOOST_NORETURN void throw_exception(const std::exception&);
BOOST_NORETURN void throw_exception(const std::exception&, const boost::source_location&);
}  // namespace boost
#endif

static_assert(std::endian::native == std::endian::little, "Target architecture MUST be little endian");

namespace zenpp {

using BlockNum = uint32_t;
using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;

template <class T>
concept UnsignedIntegral = std::unsigned_integral<T>;

template <class T>
concept SignedIntegral = std::signed_integral<T>;

template <class T>
concept Integral = UnsignedIntegral<T> or SignedIntegral<T>;

template <class T>
concept UnsignedBigIntegral = std::same_as<T, uint128_t> or std::same_as<T, uint256_t>;

//! \brief Used to allow passing string literals as template arguments
template <size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }  // NOLINT(*-explicit-constructor)
    char value[N]{};
};

//! \brief Portable strnlen_s
#if !defined(_MSC_VER)
unsigned long long strnlen_s(const char* str, size_t strsz) noexcept;
#endif

//! \brief Returns build information
const buildinfo* get_buildinfo() noexcept;

//! \brief Returns build information as string
std::string get_buildinfo_string() noexcept;

//! \brief Stores and manipulates arbitrary long byte sequences
using Bytes = std::basic_string<uint8_t>;

//! \brief Represents a non-owning view of a byte sequence
class ByteView : public std::basic_string_view<uint8_t> {
  public:
    constexpr ByteView() noexcept : std::basic_string_view<uint8_t> {}
    {};

    constexpr ByteView(const std::basic_string_view<uint8_t>& other) noexcept : std::basic_string_view<uint8_t> {
        other.data(), other.length()
    }
    {}

    constexpr ByteView(const Bytes& str) noexcept : std::basic_string_view<uint8_t> { str.data(), str.length() }
    {}

    constexpr ByteView(const uint8_t* data, size_type length) noexcept : std::basic_string_view<uint8_t> {
        data, length
    }
    {}

    template <std::size_t N>
    constexpr ByteView(const uint8_t (&array)[N]) noexcept : std::basic_string_view<uint8_t> {
        array, N
    }
    {}

    template <std::size_t N>
    constexpr ByteView(const std::array<uint8_t, N>& array) noexcept : std::basic_string_view<uint8_t> {
        array.data(), N
    }
    {}

    [[nodiscard]] bool is_null() const noexcept { return data() == nullptr; }
};

// Sizes base 10
static constexpr uint64_t kKB{1'000};        // 10^{3} bytes
static constexpr uint64_t kMB{kKB * 1'000};  // 10^{6} bytes
static constexpr uint64_t kGB{kMB * 1'000};  // 10^{9} bytes
static constexpr uint64_t kTB{kGB * 1'000};  // 10^{12} bytes

// Sizes base 2 https://en.wikipedia.org/wiki/Binary_prefix
static constexpr uint64_t kKiB{1024};        // 2^{10} bytes
static constexpr uint64_t kMiB{kKiB << 10};  // 2^{20} bytes
static constexpr uint64_t kGiB{kMiB << 10};  // 2^{30} bytes
static constexpr uint64_t kTiB{kGiB << 10};  // 2^{40} bytes

// Literals for sizes base 10
constexpr uint64_t operator"" _KB(unsigned long long value) { return value * kKB; }
constexpr uint64_t operator"" _MB(unsigned long long value) { return value * kMB; }
constexpr uint64_t operator"" _GB(unsigned long long value) { return value * kGB; }
constexpr uint64_t operator"" _TB(unsigned long long value) { return value * kTB; }

// Literals for sizes base 2
constexpr uint64_t operator"" _KiB(unsigned long long value) { return value * kKiB; }
constexpr uint64_t operator"" _MiB(unsigned long long value) { return value * kMiB; }
constexpr uint64_t operator"" _GiB(unsigned long long value) { return value * kGiB; }
constexpr uint64_t operator"" _TiB(unsigned long long value) { return value * kTiB; }

static constexpr int64_t kCoinMaxDecimals{8};         // Max number of denomination decimals
static constexpr int64_t kCoin{100'000'000};          // As many zeroes as kCoinMaxDecimals
static constexpr int64_t kCoinCent{kCoin / 100};      // One coin cent
static constexpr int64_t kCoinMaxSupply{21'000'000};  // Max tokens supply
static constexpr std::string_view kCurrency{"ZEN"};

}  // namespace zenpp
