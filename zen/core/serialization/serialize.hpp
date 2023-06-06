/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>
#include <bit>
#include <type_traits>

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/base.hpp>

//! \brief All functions dedicated to objects and types serialization
namespace zen::serialization {

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
//! \remarks Mostly used in P2P messages to prepend a list of elements with the count of elements.
//! Not to be confused with varint which is used in storage serialization
inline uint32_t ser_compact_sizeof(uint64_t value) {
    if (value < 253)
        return 1;  // One byte only
    else if (value <= 0xffff)
        return 3;  // One byte prefix + 2 bytes of uint16_t
    else if (value <= 0xffffffff)
        return 5;  // One byte prefix + 4 bytes of uint32_t
    return 9;      // One byte prefix + 8 bytes of uint64_t
}

//! \brief Lowest level serialization for integral arithmetic types
template <class Archive, std::integral T>
inline void write_data(Archive& archive, T obj) {
    std::array<unsigned char, sizeof(obj)> bytes{};
    std::memcpy(bytes.data(), &obj, sizeof(obj));
    archive.write(bytes.data(), bytes.size());
}

//! \brief Lowest level serialization for bool
template <class Archive>
inline void write_data(Archive& archive, bool obj) {
    const uint8_t out{static_cast<uint8_t>(obj ? 0x00 : 0x01)};
    archive.push_back(out);
}

//! \brief Lowest level serialization for float
template <class Archive>
inline void write_data(Archive& archive, float obj) {
    auto casted{std::bit_cast<uint32_t>(obj)};
    write_data(archive, casted);
}

//! \brief Lowest level serialization for double
template <class Archive>
inline void write_data(Archive& archive, double obj) {
    auto casted{std::bit_cast<uint64_t>(obj)};
    write_data(archive, casted);
}

//! \brief Lowest level serialization for compact integer
template <class Archive>
inline void write_compact(Archive& archive, uint64_t obj) {
    const auto casted{std::bit_cast<std::array<uint8_t, sizeof(obj)>>(obj)};
    const auto num_bytes{ser_compact_sizeof(obj)};
    uint8_t prefix{0x00};

    switch (num_bytes) {
        case 1:
            archive.push_back(casted[0]);
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
            ZEN_ASSERT(false);  // Should not happen - Houston we have a problem
    }

    archive.push_back(prefix);
    archive.write(casted.data(), num_bytes - 1 /* num_bytes count includes the prefix */);
}

//! \brief Lowest level deserialization for arithmetic types
template <typename T, class Archive>
    requires std::is_arithmetic_v<T>
inline Error read_data(Archive& archive, T& object) {
    const uint32_t count{ser_sizeof(object)};
    const auto read_result{archive.read(count)};
    if (!read_result) return read_result.error();
    std::memcpy(&object, read_result->data(), count);
    archive.shrink();  // Remove consumed data
    return Error::kSuccess;
}

//! \brief Lowest level deserialization for arithmetic types
template <typename T, class Archive>
    requires std::is_arithmetic_v<T>
inline tl::expected<T, Error> read_data(Archive& archive) {
    T ret{0};
    auto result{read_data(archive, ret)};
    if (result != Error::kSuccess) {
        return tl::unexpected{result};
    }
    return ret;
}

//! \brief Lowest level deserialization for compact integer
//! \remarks As these are primarily used to decode the size of vector-like serializations, by default a range
//! check is performed. When used as a generic number encoding, range_check should be set to false.
template <class Archive>
inline tl::expected<uint64_t, Error> read_compact(Archive& archive, bool range_check = true) {
    const auto size{read_data<uint8_t>(archive)};
    if (!size) return tl::unexpected(size.error());

    uint64_t ret{0};

    if (*size < 253) {
        ret = *size;
    } else if (*size == 253) {
        const auto value{read_data<uint16_t>(archive)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 253) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    } else if (*size == 254) {
        const auto value{read_data<uint32_t>(archive)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 0x10000UL) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    } else if (*size == 255) {
        const auto value{read_data<uint64_t>(archive)};
        if (!value) return tl::unexpected(value.error());
        if (*value < 0x100000000ULL) return tl::unexpected(Error::kNonCanonicalCompactSize);
        ret = *value;
    }
    if (range_check && ret > kMaxSerializedCompactSize) return tl::unexpected(Error::kCompactSizeTooBig);
    return ret;
}
}  // namespace zen::serialization
