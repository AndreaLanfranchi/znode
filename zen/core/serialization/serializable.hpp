/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/archive.hpp>

namespace zen::serialization {

//! \brief Public interface all serializable objects must implement
class Serializable {
  public:

    virtual ~Serializable() = default;

    [[nodiscard]] size_t serialized_size(Archive& archive) {
        std::ignore = serialization(archive, Action::kComputeSize);
        return archive.computed_size();
    }

    [[nodiscard]] serialization::Error serialize(Archive& archive) {
        return serialization(archive, Action::kSerialize);
    }

    // Needed for derived classes implementing spaceship operator
    constexpr auto operator<=>(const Serializable&) const = default;

  private:
    virtual Error serialization(Archive& archive, Action action) = 0;
};
}  // namespace zen::serialization
