/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>
#include <cstdint>
#include <ranges>

namespace znode::ser {

static constexpr uint32_t kMaxSerializedCompactSize{0x02000000};
static constexpr size_t kMaxStreamSize{512_MiB};  // As safety precaution

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
}  // namespace znode::ser
