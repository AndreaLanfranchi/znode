/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <core/common/base.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

namespace zenpp {

static constexpr size_t kBlockHeaderSerializedSize{140};  // Excluding Equihash solution

class BlockHeader : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;

    int32_t version{0};
    h256 parent_hash{};
    h256 merkle_root{};
    h256 scct_root{};
    uint32_t time{0};
    uint32_t bits{0};
    intx::uint256 nonce{0};
    Bytes solution{};

    void reset();

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp
