/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hash160.hpp>

namespace zen::crypto {

Hash160::Hash160(ByteView data) : Hash160() { init(data); }

Hash160::Hash160(std::string_view data) : Hash160() { init(data); }

void Hash160::init() noexcept { inner_.init(); }

void Hash160::init(ByteView data) noexcept { inner_.init(data); }

void Hash160::init(std::string_view data) noexcept { inner_.init(data); }

void Hash160::update(ByteView data) noexcept { inner_.update(data); }

void Hash160::update(std::string_view data) noexcept { inner_.update(data); }

Bytes Hash160::finalize() noexcept {
    Bytes tmp{inner_.finalize()};
    if (tmp.empty()) return tmp;
    Ripemd160 outer(tmp);
    return outer.finalize();
}
}  // namespace zen::crypto
