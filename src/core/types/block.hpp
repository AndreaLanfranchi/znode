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
#include <core/common/base.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

namespace znode {

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

    //! \brief Reset the object to its default state
    void reset();

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};
}  // namespace znode
