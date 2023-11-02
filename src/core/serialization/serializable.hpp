/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <core/common/base.hpp>
#include <core/serialization/stream.hpp>

#if defined(_MSC_VER)
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#include <byteswap.h>
#endif

namespace znode::ser {

//! \brief Public interface all serializable objects must implement
class Serializable {
  public:
    Serializable() = default;
    virtual ~Serializable() = default;

    [[nodiscard]] size_t serialized_size(SDataStream& stream) {
        const auto result{serialization(stream, Action::kComputeSize)};
        if (not result) return 0U;
        return stream.computed_size();
    }

    [[nodiscard]] outcome::result<void> serialize(SDataStream& stream) {
        return serialization(stream, Action::kSerialize);
    }

    [[nodiscard]] outcome::result<void> deserialize(SDataStream& stream) {
        return serialization(stream, Action::kDeserialize);
    }

    // Needed for derived classes implementing spaceship operator
    constexpr auto operator<=>(const Serializable&) const = default;

  private:
    virtual outcome::result<void> serialization(SDataStream& stream, Action action) = 0;
};
}  // namespace znode::ser
