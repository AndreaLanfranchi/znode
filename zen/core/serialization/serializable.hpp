/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/stream.hpp>

namespace zen::serialization {

//! \brief Public interface all serializable objects must implement
class Serializable {
  public:
    virtual ~Serializable() = default;

    [[nodiscard]] size_t serialized_size(SDataStream& stream) {
        std::ignore = serialization(stream, Action::kComputeSize);
        return stream.computed_size();
    }

    [[nodiscard]] serialization::Error serialize(SDataStream& stream) {
        return serialization(stream, Action::kSerialize);
    }

    [[nodiscard]] serialization::Error deserialize(SDataStream& stream) {
        return serialization(stream, Action::kDeserialize);
    }

    // Needed for derived classes implementing spaceship operator
    constexpr auto operator<=>(const Serializable&) const = default;

  private:
    virtual Error serialization(SDataStream& stream, Action action) = 0;
};
}  // namespace zen::serialization
