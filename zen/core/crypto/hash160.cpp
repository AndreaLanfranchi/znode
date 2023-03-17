/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hash160.hpp>
#include <zen/core/crypto/ripemd.hpp>

namespace zen::crypto {

Hash160::Hash160() : Hasher(RIPEMD160_DIGEST_LENGTH, SHA256_CBLOCK) {}

Hash160::Hash160(ByteView initial_data) : Hash160() { init(initial_data); }

Hash160::Hash160(std::string_view initial_data) : Hash160() { init(string_view_to_byte_view(initial_data)); }

void Hash160::init() noexcept { hasher.init(); }

void Hash160::init(ByteView initial_data) noexcept {
    init();
    hasher.update(initial_data);
}
void Hash160::update(ByteView data) noexcept { hasher.update(data); }

void Hash160::update(std::string_view data) noexcept { hasher.update(data); }

Bytes Hash160::finalize() noexcept {
    SecureBytes buffer(hasher.digest_size(), 0);
    buffer.assign(hasher.finalize());
    Ripemd160 hasher2({&buffer[0], buffer.size()});
    return hasher2.finalize();
}

}  // namespace zen::crypto
