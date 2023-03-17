/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/assert.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/endian.hpp>
#include <zen/core/crypto/sha_2_512.hpp>

namespace zen::crypto {

ZEN_THREAD_LOCAL ObjectPool<SHA512_CTX> Sha512::ctx_pool_{};

Sha512::Sha512() : Hasher(SHA512_DIGEST_LENGTH, SHA512_CBLOCK) { init_context(); }

Sha512::~Sha512() {
    if (ctx_) {
        ctx_pool_.add(ctx_.release());
    }
}

Sha512::Sha512(ByteView initial_data) : Sha512() { update(initial_data); }
Sha512::Sha512(std::string_view initial_data) : Sha512(string_view_to_byte_view(initial_data)) {}

void Sha512::init() noexcept { init_context(); }

Bytes Sha512::finalize() noexcept {
    static const std::array<uint8_t, SHA512_CBLOCK> pad{0x80};

    Bytes sizedesc(sizeof(uint64_t) * 2, '\0');
    endian::store_big_u64(&sizedesc[8], total_bytes_ << 3);
    update({&pad[0], 1 + ((239 - (total_bytes_ % block_size_)) % block_size_)});
    update({&sizedesc[0], sizedesc.size()});
    return finalize_nopadding(false);
}

Bytes Sha512::finalize_nopadding(bool compression) const noexcept {
    if (compression) {
        ZEN_ASSERT(total_bytes_ == SHA512_CBLOCK);
    }

    Bytes ret(digest_size_, '\0');
    for (size_t i{0}; i < 8; ++i) {
        endian::store_big_u64(&ret[i << 3], ctx_->h[i]);
    }
    return ret;
}

void Sha512::init_context() {
    if (!ctx_) {
        ctx_.reset(ctx_pool_.acquire());
        if (!ctx_) ctx_ = std::make_unique<SHA512_CTX>();
        ZEN_ASSERT(ctx_.get() != nullptr);
    }
    SHA512_Init(ctx_.get());
    total_bytes_ = 0;
    buffer_offset_ = 0;
}

void Sha512::transform(const unsigned char* data) { SHA512_Transform(ctx_.get(), data); }
}  // namespace zen::crypto
