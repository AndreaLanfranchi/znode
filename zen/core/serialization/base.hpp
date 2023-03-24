/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

namespace zen::serialization {

static constexpr uint32_t kMaxSerializedCompactSize{0x02000000};

//! \brief Scopes of serialization/deserialization
enum class Scope : uint32_t {
    kNetwork = (1 << 0),
    kStorage = (1 << 1),
    kHash = (1 << 2)
};

//! \brief Specifies the serialization action
enum class Action : uint32_t {
    kComputeSize = (1 << 0),  // Only calculates size of serialized data
    kSerialize = (1 << 1),    // Actually performs serialization
    kDeserialize = (1 << 2)   // Deserializes data into object
};

enum class Error {
    kSuccess,  // Actually not an error
    kReadBeyondData,
    kNonCanonicalCompactSize,
    kCompactSizeTooBig,
};

inline bool operator!(Error e) { return e == static_cast<Error>(0); }

}  // namespace zen::serialization
