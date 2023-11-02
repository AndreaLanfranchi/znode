/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once
#include <nlohmann/json.hpp>

#include <core/common/base.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

namespace znode {

class InventoryItem : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;

    enum class Type : uint32_t {
        kError = 0,
        kTx = 1,
        kBlock = 2,
        kFilteredBlock = 3,
        // TODO Not supported yet kCmpactBlock = 4,
        // TODO Not supported yet kWitnessTx = 0x40000001,
        // TODO Not supported yet kWitnessBlock = 0x40000002,
        // TODO Not supported yet kFilteredWitnessBlock = 0x40000003
    };

    Type type_{Type::kError};
    h256 identifier_{};

    //! \brief Reset the object to its default state
    void reset();

    //! \brief Return a JSON representation of the object
    [[nodiscard]] nlohmann::json to_json() const noexcept;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};
}  // namespace znode
