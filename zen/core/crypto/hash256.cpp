/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hash256.hpp>

namespace zen::crypto {

Hash256::Hash256() : Hasher(SHA256_DIGEST_LENGTH, SHA256_CBLOCK) {}

Hash256::Hash256(ByteView initial_data) : Hash256() { init(initial_data); }

Hash256::Hash256(std::string_view initial_data) : Hash256() { init(string_view_to_byte_view(initial_data)); }

void Hash256::init() noexcept { hasher.init(); }

void Hash256::init(ByteView initial_data) noexcept {
    init();
    hasher.update(initial_data);
}
void Hash256::update(ByteView data) noexcept { hasher.update(data); }

void Hash256::update(std::string_view data) noexcept { hasher.update(data); }

Bytes Hash256::finalize() noexcept {
    SecureBytes buffer(hasher.digest_size(), 0);
    buffer.assign(hasher.finalize());
    hasher.init();
    hasher.update({&buffer[0], buffer.size()});
    return hasher.finalize();
}

}  // namespace zen::crypto
