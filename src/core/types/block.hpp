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

class BlockHeader : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;

    int32_t version{0};  // 4 bytes
    h256 parent_hash{};  // 32 bytes
    h256 merkle_root{};  // 32 bytes
    h256 scct_root{};    // 32 bytes
    uint32_t time{0};    // 4 bytes
    uint32_t bits{0};    // 4 bytes
    uint256_t nonce{0};  // 32 bytes Total: 140 bytes
    Bytes solution{};

    void reset();

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};
}  // namespace zenpp
