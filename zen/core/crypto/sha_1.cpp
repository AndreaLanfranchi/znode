/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/assert.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/endian.hpp>
#include <zen/core/crypto/sha_1.hpp>

namespace zen::crypto {

ZEN_THREAD_LOCAL ObjectPool<SHA_CTX> Sha1::ctx_pool_{};

Sha1::Sha1() : Hasher(SHA_DIGEST_LENGTH, SHA_CBLOCK) { init_context(); }

Sha1::~Sha1() {
    if (ctx_) {
        ctx_pool_.add(ctx_.release());
    }
}
Sha1::Sha1(ByteView initial_data) : Sha1() { update(initial_data); }
Sha1::Sha1(std::string_view initial_data) : Sha1(string_view_to_byte_view(initial_data)) {}

void Sha1::init() noexcept { init_context(); }

Bytes Sha1::finalize() noexcept {
    static const std::array<uint8_t, SHA_CBLOCK> pad{0x80};

    Bytes sizedesc(sizeof(uint64_t), '\0');
    endian::store_big_u64(&sizedesc[0], total_bytes_ << 3);
    update({&pad[0], 1 + ((119 - (total_bytes_ % block_size_)) % block_size_)});
    update({&sizedesc[0], sizedesc.size()});

    Bytes ret(digest_size_, '\0');
    endian::store_big_u32(&ret[0], ctx_->h0);
    endian::store_big_u32(&ret[4], ctx_->h1);
    endian::store_big_u32(&ret[8], ctx_->h2);
    endian::store_big_u32(&ret[12], ctx_->h3);
    endian::store_big_u32(&ret[16], ctx_->h4);
    return ret;
}

void Sha1::init_context() {
    if (!ctx_) {
        ctx_.reset(ctx_pool_.acquire());
        if (!ctx_) ctx_ = std::make_unique<SHA_CTX>();
        ZEN_ASSERT(ctx_.get() != nullptr);
    }
    SHA1_Init(ctx_.get());
    total_bytes_ = 0;
    buffer_offset_ = 0;
}

void Sha1::transform(const unsigned char* data) { SHA1_Transform(ctx_.get(), data); }

}  // namespace zen::crypto
