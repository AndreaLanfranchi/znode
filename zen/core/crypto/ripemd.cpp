/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/assert.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/endian.hpp>
#include <zen/core/crypto/ripemd.hpp>

namespace zen::crypto {

ZEN_THREAD_LOCAL ObjectPool<RIPEMD160_CTX> Ripemd160::ctx_pool_{};

Ripemd160::Ripemd160() : Hasher(RIPEMD160_DIGEST_LENGTH, RIPEMD160_CBLOCK) { init_context(); }

Ripemd160::~Ripemd160() {
    if (ctx_) {
        ctx_pool_.add(ctx_.release());
    }
}
Ripemd160::Ripemd160(ByteView initial_data) : Ripemd160() { update(initial_data); }
Ripemd160::Ripemd160(std::string_view initial_data) : Ripemd160(string_view_to_byte_view(initial_data)) {}

void Ripemd160::init() noexcept { init_context(); }

Bytes Ripemd160::finalize() noexcept {
    static const std::array<uint8_t, RIPEMD160_CBLOCK> pad{0x80};

    Bytes sizedesc(sizeof(uint64_t), '\0');
    endian::store_little_u64(&sizedesc[0], total_bytes_ << 3);
    update({&pad[0], 1 + ((119 - (total_bytes_ % block_size_)) % block_size_)});
    update({&sizedesc[0], sizedesc.size()});

    Bytes ret(digest_size_, '\0');
    endian::store_little_u32(&ret[0], ctx_->A);
    endian::store_little_u32(&ret[4], ctx_->B);
    endian::store_little_u32(&ret[8], ctx_->C);
    endian::store_little_u32(&ret[12], ctx_->D);
    endian::store_little_u32(&ret[16], ctx_->E);
    return ret;
}

void Ripemd160::init_context() {
    if (!ctx_) {
        ctx_.reset(ctx_pool_.acquire());
        if (!ctx_) ctx_ = std::make_unique<RIPEMD160_CTX>();
        ZEN_ASSERT(ctx_.get() != nullptr);
    }
    RIPEMD160_Init(ctx_.get());
    total_bytes_ = 0;
    buffer_offset_ = 0;
}

void Ripemd160::transform(const unsigned char* data) { RIPEMD160_Transform(ctx_.get(), data); }
}  // namespace zen::crypto
