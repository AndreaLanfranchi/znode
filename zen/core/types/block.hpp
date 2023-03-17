/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <zen/core/common/base.hpp>
#include <zen/core/types/hash.hpp>

namespace zen {

struct BlockHeader {
    int32_t version;
    h256 parent_hash;
    h256 merkle_root;
    h256 sidechains_commitment_root;
    uint32_t time;
    uint32_t bits;
};

}  // namespace zen
