/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hash256.hpp>

namespace zen::crypto {

Hash256::Hash256(ByteView data) : Hash256() { hasher.init(data); }

Hash256::Hash256(std::string_view data) : Hash256() { hasher.init(data); }

void Hash256::init() noexcept { hasher.init(); }

void Hash256::init(ByteView data) noexcept { hasher.init(data); }

void Hash256::init(std::string_view data) noexcept { hasher.init(data); }

void Hash256::update(ByteView data) noexcept { hasher.update(data); }

void Hash256::update(std::string_view data) noexcept { hasher.update(data); }

Bytes Hash256::finalize() noexcept {
    Bytes tmp{hasher.finalize()};
    if (tmp.empty()) return tmp;
    hasher.init(tmp);
    return hasher.finalize();
}
}  // namespace zen::crypto
