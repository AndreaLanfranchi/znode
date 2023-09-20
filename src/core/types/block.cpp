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

ser::Error BlockHeader::serialization(ser::SDataStream& stream, ser::Action action) {
    using namespace ser;
    using enum Error;
    Error error{stream.bind(version, action)};
    if (!error) stream.set_version(version);
    if (!error) error = stream.bind(parent_hash, action);
    if (!error) error = stream.bind(merkle_root, action);
    if (!error) error = stream.bind(scct_root, action);
    if (!error) error = stream.bind(time, action);
    if (!error) error = stream.bind(bits, action);
    if (!error) error = stream.bind(nonce, action);
    if (!error) error = stream.bind(solution, action);
    return error;
}
}  // namespace zenpp
