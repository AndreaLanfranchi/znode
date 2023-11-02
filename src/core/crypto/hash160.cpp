/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "hash160.hpp"

#include <core/common/cast.hpp>

namespace znode::crypto {

Hash160::Hash160(ByteView data) : hasher_(data) {}

Hash160::Hash160(std::string_view data) : hasher_(data) {}

void Hash160::init() noexcept { hasher_.init(); }

void Hash160::init(ByteView data) noexcept { hasher_.init(data); }

void Hash160::init(std::string_view data) noexcept { hasher_.init(data); }

void Hash160::update(ByteView data) noexcept { hasher_.update(data); }

void Hash160::update(std::string_view data) noexcept { hasher_.update(data); }

Bytes Hash160::finalize() noexcept {
    if (hasher_.ingested_size() == 0U) return kEmptyHash();
    Bytes data{hasher_.finalize()};
    if (data.empty()) return data;  // Some error occurred
    Ripemd160 outer(data);
    return outer.finalize();
}
}  // namespace znode::crypto
