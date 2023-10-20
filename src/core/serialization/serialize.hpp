/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>
#include <bit>
#include <type_traits>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>
#include <core/serialization/base.hpp>
#include <core/serialization/errors.hpp>

//! \brief All functions dedicated to objects and types serialization
namespace zenpp::ser {

//! \brief ssizeof stands for serialized size of. Returns the serialized size of arithmetic types
//! \remarks Do not define serializable classes members as size_t as it might lead to wrong results on
//! MacOS/Xcode bundles
//! \remarks The size of big integers derived from boost cpp_int cannot be determined by sizeof as
//! it's always at least one machine word greater than you would expect for an N-bit integer
//! As a result the specialization for uint128_t and uint256_t is required
//! https://www.boost.org/doc/libs/1_81_0/libs/multiprecision/doc/html/boost_multiprecision/tut/ints/cpp_int.html
template <typename T>
inline constexpr uint32_t ssizeof = sizeof(T);
template <>
inline constexpr uint32_t ssizeof<bool> = 1U;
template <>
inline constexpr uint32_t ssizeof<uint128_t> = 16U;
template <>
inline constexpr uint32_t ssizeof<uint256_t> = 32U;

//! \brief Returns the serialzed size of a compacted integral
//! \remarks Mostly used in P2P messages to prepend a list of elements with the count of items to be expected.
//! \attention Not to be confused with varint
inline constexpr uint32_t ser_compact_sizeof(uint64_t value) {
    if (value < (UINT8_MAX - 2U)) return 1U;  // One byte only
    if (value <= UINT16_MAX) return 3U;       // One byte prefix + 2 bytes of uint16_t
    if (value <= UINT32_MAX) return 5U;       // One byte prefix + 4 bytes of uint32_t
    return 9U;                                // One byte prefix + 8 bytes of uint64_t
}

//! \brief Lowest level serialization for integral arithmetic types
template <class Stream, Integral T>
inline outcome::result<void> write_data(Stream& stream, T obj) {
    const auto bytes{std::bit_cast<std::array<uint8_t, sizeof(obj)>>(obj)};
    return stream.write(bytes.data(), bytes.size());
}

//! \brief Lowest level serialization for bool
template <class Stream>
inline outcome::result<void> write_data(Stream& stream, bool obj) {
    const uint8_t byte{static_cast<uint8_t>(obj ? 0x01 : 0x00)};
    stream.push_back(byte);
    return outcome::success();
}

//! \brief Lowest level serialization for float
template <class Stream>
inline outcome::result<void> write_data(Stream& stream, float obj) {
    auto casted{std::bit_cast<uint32_t>(obj)};
    return write_data(stream, casted);
}

//! \brief Lowest level serialization for double
template <class Stream>
inline outcome::result<void> write_data(Stream& stream, double obj) {
    auto casted{std::bit_cast<uint64_t>(obj)};
    return write_data(stream, casted);
}

//! \brief Lowest level serialization for compact integer
template <class Stream>
inline outcome::result<void> write_compact(Stream& stream, uint64_t obj) {
    const auto casted{std::bit_cast<std::array<uint8_t, sizeof(obj)>>(obj)};
    const auto num_bytes{ser_compact_sizeof(obj)};
    uint8_t prefix{0x00};

    switch (num_bytes) {
        case 1:
            stream.push_back(casted[0]);
            return outcome::success();
        case 3:
            prefix = 253;
            break;
        case 5:
            prefix = 254;
            break;
        case 9:
            prefix = 255;
            break;
        default:
            ASSERT(false && "Should not happen");
    }

    stream.push_back(prefix);
    return stream.write(casted.data(), num_bytes - 1 /* num_bytes count includes the prefix */);
}

//! \brief Lowest level deserialization for arithmetic types
template <class Stream, Integral T>
inline outcome::result<void> read_data(Stream& stream, T& object) {
    const uint32_t count{ssizeof<T>};
    const auto read_result{stream.read(count)};
    if (read_result.has_error()) return read_result.error();
    std::memcpy(&object, read_result.value().data(), count);
    return outcome::success();
}

//! \brief Lowest level deserialization for bool
template <class Stream>
inline outcome::result<void> read_data(Stream& stream, bool& object) {
    const auto read_result{stream.read(ssizeof<bool>)};
    if (read_result.has_error()) return read_result.error();
    object = (read_result.value().data()[0] == 0x01);
    return outcome::success();
}

//! \brief Lowest level deserialization for arithmetic types
template <typename T, class Stream>
    requires Integral<T>
inline outcome::result<T> read_as(Stream& stream) {
    T ret{0};
    if (const outcome::result<T> read_result{read_data(stream, ret)}; read_result.has_error()) {
        return read_result.error();
    }
    return outcome::success<T>(ret);
}

template <typename T, class Stream>
    requires std::same_as<T, double> or std::same_as<T, float>
inline outcome::result<T> read_as(Stream& stream) {
    const auto bytes_to_read{ssizeof<T>};
    const auto read_result{stream.read(bytes_to_read)};
    if (read_result.has_error()) return read_result.error();
    if constexpr (std::same_as<T, double>) {
        double ret{0.0};
        std::memcpy(&ret, read_result.value().data(), bytes_to_read);
        return outcome::success<double>(ret);
    } else {
        float ret{0.0F};
        std::memcpy(&ret, read_result.value().data(), bytes_to_read);
        return outcome::success<float>(ret);
    }
}

//! \brief Lowest level deserialization for compact integer
//! \remarks As these are primarily used to decode the size of vector-like serializations, by default a range
//! check is performed. When used as a generic number encoding, range_check should be set to false.
template <class Stream>
inline outcome::result<uint64_t> read_compact(Stream& stream, bool range_check = true) {
    const outcome::result<uint8_t> compact_prefix{read_as<uint8_t>(stream)};
    if (compact_prefix.has_error()) return compact_prefix.error();

    uint64_t ret{0};
    if (compact_prefix.value() < 253) {
        ret = compact_prefix.value();
    } else if (compact_prefix.value() == 253) {
        if (const auto read_result{read_as<uint16_t>(stream)}; read_result.has_error()) {
            return read_result.error();
        } else {
            if (read_result.value() < 253U) return Error::kNonCanonicalCompactSize;
            ret = read_result.value();
        }
    } else if (compact_prefix.value() == 254) {
        if (const auto read_result{read_as<uint32_t>(stream)}; read_result.has_error()) {
            return read_result.error();
        } else {
            if (read_result.value() < 0x10000UL) return Error::kNonCanonicalCompactSize;
            ret = read_result.value();
        }
    } else if (compact_prefix.value() == 255) {
        if (const auto read_result{read_as<uint64_t>(stream)}; read_result.has_error()) {
            return read_result.error();
        } else {
            if (read_result.value() < 0x100000000ULL) return Error::kNonCanonicalCompactSize;
            ret = read_result.value();
        }
    }
    if (range_check and ret > kMaxSerializedCompactSize) return Error::kCompactSizeTooBig;
    return ret;
}
}  // namespace zenpp::ser
