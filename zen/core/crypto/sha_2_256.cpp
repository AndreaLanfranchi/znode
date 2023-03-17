/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/assert.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/endian.hpp>
#include <zen/core/crypto/sha_2_256.hpp>

namespace zen::crypto {

ZEN_THREAD_LOCAL ObjectPool<SHA256_CTX> Sha256::ctx_pool_{};

Sha256::Sha256() : Hasher(SHA256_DIGEST_LENGTH, SHA256_CBLOCK) { init_context(); }

Sha256::~Sha256() {
    if (ctx_) {
        ctx_pool_.add(ctx_.release());
    }
}

Sha256::Sha256(ByteView initial_data) : Sha256() { update(initial_data); }
Sha256::Sha256(std::string_view initial_data) : Sha256(string_view_to_byte_view(initial_data)) {}

void Sha256::init() noexcept { init_context(); }

Bytes Sha256::finalize() noexcept {
    static const std::array<uint8_t, SHA256_CBLOCK> pad{0x80};

    Bytes sizedesc(sizeof(uint64_t), '\0');
    endian::store_big_u64(&sizedesc[0], total_bytes_ << 3);
    update({&pad[0], 1 + ((119 - (total_bytes_ % block_size_)) % block_size_)});
    update({&sizedesc[0], sizedesc.size()});
    return finalize_nopadding(false);
}
Bytes Sha256::finalize_nopadding(bool compression) const noexcept {
    if (compression) {
        ZEN_ASSERT(total_bytes_ == block_size_);
    }

    Bytes ret(digest_size_, '\0');
    for (size_t i{0}; i < 8; ++i) {
        endian::store_big_u32(&ret[i << 2], ctx_->h[i]);
    }
    return ret;
}

void Sha256::init_context() {
    if (!ctx_) {
        ctx_.reset(ctx_pool_.acquire());
        if (!ctx_) ctx_ = std::make_unique<SHA256_CTX>();
        ZEN_ASSERT(ctx_.get() != nullptr);
    }
    SHA256_Init(ctx_.get());
    total_bytes_ = 0;
    buffer_offset_ = 0;
}

void Sha256::transform(const unsigned char* data) { SHA256_Transform(ctx_.get(), data); }

}  // namespace zen::crypto
