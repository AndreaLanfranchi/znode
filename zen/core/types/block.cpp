/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/types/block.hpp>

namespace zen {

void BlockHeader::reset() {
    version = 0;
    parent_hash.reset();
    merkle_root.reset();
    scct_root.reset();
    time = 0;
    bits = 0;
    nonce = 0;
    solution.clear();
}
}  // namespace zen
