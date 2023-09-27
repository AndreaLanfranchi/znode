/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "block.hpp"

namespace zenpp {

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

outcome::result<void> BlockHeader::serialization(ser::SDataStream& stream, ser::Action action) {
    auto result{stream.bind(version, action)};
    if (not result.has_error()) stream.set_version(version);
    if (not result.has_error()) result = stream.bind(parent_hash, action);
    if (not result.has_error()) result = stream.bind(merkle_root, action);
    if (not result.has_error()) result = stream.bind(scct_root, action);
    if (not result.has_error()) result = stream.bind(time, action);
    if (not result.has_error()) result = stream.bind(bits, action);
    if (not result.has_error()) result = stream.bind(nonce, action);
    if (not result.has_error()) result = stream.bind(solution, action);
    return result;
}
}  // namespace zenpp
