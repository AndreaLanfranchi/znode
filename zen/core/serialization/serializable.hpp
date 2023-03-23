/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/stream.hpp>

namespace zen::ser {

//! \brief Public interface all serializable objects must implement
class Serializable {
  public:
    [[nodiscard]] size_t serialized_size(Archive& stream) {
        serialization(stream, stream.scope(), Action::kComputeSize);
        return stream.computed_size();
    }

    void serialize(Archive& stream) { serialization(stream, stream.scope(), Action::kSerialize); }

  private:
    friend class Archive;
    virtual void serialization(Archive& stream, Scope scope, Action action) = 0;
};
}  // namespace zen::ser
