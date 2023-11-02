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

#include "block.hpp"

namespace znode {

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
}  // namespace znode
