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
    Serializable() = default;

    //! \brief Returns the size (in bytes) the serialized form will have
    virtual size_t serialized_size(ser::DataStream& stream) = 0;

    virtual void serialize(ser::DataStream& stream) = 0;
    virtual void deserialize(ser::DataStream&) = 0;
};
}  // namespace zen::ser
