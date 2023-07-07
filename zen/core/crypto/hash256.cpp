/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hash256.hpp>

namespace zen::crypto {

Hash256::Hash256(ByteView data) : hasher_(data) {}

Hash256::Hash256(std::string_view data) : hasher_(data) {}

void Hash256::init() noexcept { hasher_.init(); }

void Hash256::init(ByteView data) noexcept { hasher_.init(data); }

void Hash256::init(std::string_view data) noexcept { hasher_.init(data); }

void Hash256::update(ByteView data) noexcept { hasher_.update(data); }

void Hash256::update(std::string_view data) noexcept { hasher_.update(data); }

Bytes Hash256::finalize() noexcept {
    if (!hasher_.ingested_size()) return kEmptyHash();
    Bytes tmp{hasher_.finalize()};
    if (tmp.empty()) return tmp;
    hasher_.init(tmp);
    return hasher_.finalize();
}
}  // namespace zen::crypto
