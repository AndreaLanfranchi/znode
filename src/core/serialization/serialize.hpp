/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>
#include <bit>
#include <type_traits>

#include <core/common/base.hpp>
#include <core/serialization/base.hpp>

//! \brief All functions dedicated to objects and types serialization
namespace zenpp::serialization {

//! \brief Returns the serialized size of arithmetic types
//! \remarks Do not define serializable classes members as size_t as it might lead to wrong results on
//! MacOS/Xcode bundles
template <class T>
    requires std::is_arithmetic_v<T>
inline uint32_t ser_sizeof(T obj) {
    return sizeof(obj);
}

//! \brief Returns the serialized size of arithmetic types
//! \remarks Specialization for bool which is stored in at least 1 byte
template <>
inline uint32_t ser_sizeof(bool) {
    return sizeof(char);
}

//! \brief Returns the serialzed size of a compacted integral
//! \remarks Mostly used in P2P messages to prepend a list of elements with the count of items to be expected.
//! \attention Not to be confused with varint
inline constexpr uint32_t ser_compact_sizeof(uint64_t value) {
    if (value < 253)
        return 1;  // One byte only
    else if (value <= 0xffff)
        return 3;  // One byte prefix + 2 bytes of uint16_t
    else if (value <= 0xffffffff)
        return 5;  // One byte prefix + 4 bytes of uint32_t
    return 9;      // One byte prefix + 8 bytes of uint64_t
}

//! \brief Lowest level serialization for integral arithmetic types
template <class Stream, std::integral T>
inline void write_data(Stream& stream, T obj) {
    std::array<unsigned char, sizeof(obj)> bytes{};
    std::memcpy(bytes.data(), &obj, sizeof(obj));
    stream.write(bytes.data(), bytes.size());
}

//! \brief Lowest level serialization for bool
template <class Stream>
inline void write_data(Stream& stream, bool obj) {
    const uint8_t out{static_cast<uint8_t>(obj ? 0x01 : 0x00)};
    stream.push_back(out);
}

//! \brief Lowest level serialization for float
template <class Stream>
inline void write_data(Stream& stream, float obj) {
    auto casted{std::bit_cast<uint32_t>(obj)};
    write_data(stream, casted);
}

//! \brief Lowest level serialization for double
template <class Stream>
inline void write_data(Stream& stream, double obj) {
    auto casted{std::bit_cast<uint64_t>(obj)};
    write_data(stream, casted);
}

//! \brief Lowest level serialization for compact integer
template <class Stream>
inline void write_compact(Stream& stream, uint64_t obj) {
    const auto casted{std::bit_cast<std::array<uint8_t, sizeof(obj)>>(obj)};
    const auto num_bytes{ser_compact_sizeof(obj)};
    uint8_t prefix{0x00};

    switch (num_bytes) {
        case 1:
            stream.push_back(casted[0]);
            return;  // Nothing else to append
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
            ASSERT(false);  // Should not happen - Houston we have a problem
    }

    stream.push_back(prefix);
    stream.write(casted.data(), num_bytes - 1 /* num_bytes count includes the prefix */);
}

//! \brief Lowest level deserialization for arithmetic types
template <typename T, class Stream>
    requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
inline Error read_data(Stream& stream, T& object) {
    const uint32_t count{ser_sizeof(object)};
    const auto read_result{stream.read(count)};
    if (!read_result) return read_result.error();
    std::memcpy(&object, read_result->data(), count);
    return Error::kSuccess;
}

//! \brief Lowest level deserialization for arithmetic types
template <typename T, class Stream>
    requires std::is_arithmetic_v<T>
inline tl::expected<T, Error> read_data(Stream& stream) {
    T ret{0};
    auto result{read_data(stream, ret)};
    if (result != Error::kSuccess) {
        return tl::unexpected{result};
    }
    return ret;
}

//! \brief Lowest level deserialization for bool
template <typename T, class Stream>
    requires std::is_same_v<T, bool>
inline Error read_data(Stream& stream, T& object) {
    const auto read_result{stream.read(1)};
    if (!read_result) return read_result.error();
    object = (read_result->data()[0] == 0x01);
    return Error::kSuccess;
}

//! \brief Lowest level deserialization for compact integer
//! \remarks As these are primarily used to decode the size of vector-like serializations, by default a range
//! check is performed. When used as a generic number encoding, range_check should be set to false.
template <class Stream>
inline tl::expected<uint64_t, Error> read_compact(Stream& stream, bool range_check = true) {
    const auto size{read_data<uint8_t>(stream)};
    if (!size) return tl::unexpected(size.error());

    uint64_t ret{0};

    if (*size < 253) {
        ret = *size;
    } else if (*size == 253) {
        const auto value{read_data<uint16_t>(stream)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 253) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    } else if (*size == 254) {
        const auto value{read_data<uint32_t>(stream)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 0x10000UL) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    } else if (*size == 255) {
        const auto value{read_data<uint64_t>(stream)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 0x100000000ULL) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    }
    if (range_check && ret > kMaxSerializedCompactSize) return tl::unexpected(Error::kCompactSizeTooBig);
    return ret;
}
}  // namespace zenpp::serialization
