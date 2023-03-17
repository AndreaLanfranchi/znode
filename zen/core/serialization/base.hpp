/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

namespace zen::ser {

static constexpr uint32_t kMaxSerializedCompactSize{0x02000000};

//! \brief Scopes of serialization/deserialization
enum class Scope : uint32_t {
    kNetwork = (1 << 0),
    kStorage = (1 << 1),
    kHash = (1 << 2)
};

enum class SerializationError {
    kSuccess,
};

enum class DeserializationError {
    kSuccess,
    kReadBeyondData,
    kNonCanonicalCompactSize,
    kCompactSizeTooBig,
};
}  // namespace zen::ser
