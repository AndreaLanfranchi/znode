/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
