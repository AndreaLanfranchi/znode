/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once
#include <array>
#include <memory>
#include <span>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/endian.hpp>
#include <core/common/object_pool.hpp>
#include <core/common/random.hpp>

namespace znode::crypto {

//! \brief Explicit deleter for OSSL_LIB_CTXes
struct LibCtxDeleter {
    constexpr LibCtxDeleter() noexcept = default;
    void operator()(OSSL_LIB_CTX* ptr) const noexcept { OSSL_LIB_CTX_free(ptr); }
};

//! \brief A collection of recycle-able OSSL_LIB_CTXes
static ObjectPool<OSSL_LIB_CTX, LibCtxDeleter> LibCtxs(/*thread_safe=*/true);

//! \brief Explicit recycler for OSSL_LIB_CTXes
struct LibCtxRecycler {
    constexpr LibCtxRecycler() noexcept = default;
    void operator()(OSSL_LIB_CTX* ptr) const noexcept { LibCtxs.add(ptr); }
};

//! \brief Explicit deleter for EVP_MAC_CTXes
struct MacCtxDeleter {
    constexpr MacCtxDeleter() noexcept = default;
    void operator()(EVP_MAC_CTX* ptr) const noexcept { EVP_MAC_CTX_free(ptr); }
};

//! \brief Explicit deleter for EVP_MACes
struct MacDeleter {
    constexpr MacDeleter() noexcept = default;
    void operator()(EVP_MAC* ptr) const noexcept { EVP_MAC_free(ptr); }
};

template <unsigned int MAC_LEN, unsigned int C_ROUNDS, unsigned int D_ROUNDS>
class SipHash {
  public:
    SipHash() { init(); }
    explicit SipHash(const ByteView key) { init(key); }
    explicit SipHash(const std::string_view key) { init(key); }
    SipHash(uint64_t k0, uint64_t k1) {
        Bytes key(2 * sizeof(k1), 0);
        endian::store_little_u64(key.data(), k0);
        endian::store_little_u64(key.data() + sizeof(k0), k1);
        init(key);
    }

    // Not copyable nor movable
    SipHash(const SipHash&) = delete;
    SipHash(const SipHash&&) = delete;
    SipHash& operator=(const SipHash&) = delete;
    SipHash(SipHash&&) = delete;

    ~SipHash() = default;

    void init() noexcept {
        auto key{get_random_bytes(2 * sizeof(uint64_t))};
        init(key);
    }

    void init(ByteView key) noexcept {
        ingested_size_ = 0;
        ASSERT(key.size() == 16);
        ASSERT(EVP_MAC_init(ctx_.get(), key.data(), key.size(), params_.data()) == 1);
    }

    void init(std::string_view key) noexcept { init(string_view_to_byte_view(key)); }

    //! \brief Accumulates more data
    void update(ByteView data) noexcept {
        if (data.empty()) return;
        ingested_size_ += data.size();
        ASSERT(EVP_MAC_update(ctx_.get(), data.data(), data.size()) == 1);
    }

    //! \brief Accumulates more data
    void update(std::string_view data) noexcept { update(string_view_to_byte_view(data)); }

    //! \brief Accumulates more data
    void update(const std::span<const std::byte> data) noexcept {
        if (data.empty()) return;
        ingested_size_ += data.size();
        ASSERT(EVP_MAC_update(ctx_.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size()) == 1);
    }

    //! \brief Accumulates more data
    template <Integral V>
    void update(V data) noexcept {
        update(ByteView(reinterpret_cast<const uint8_t*>(&data), sizeof(V)));
    }

    [[nodiscard]] Bytes finalize() noexcept {
        Bytes mac(mac_len_, 0);
        ASSERT(EVP_MAC_final(ctx_.get(), mac.data(), nullptr, mac.size()) == 1);
        return mac;
    }

    //! \brief Returns the algorithm name
    [[nodiscard]] std::string algo_name() const noexcept { return {"SIPHASH"}; }

    //! \brief Returns the size (in bytes) of the output mac
    [[nodiscard]] size_t mac_size() const noexcept { return static_cast<size_t>(mac_len_); }

    //! \brief Returns the number of bytes already digested
    [[nodiscard]] size_t ingested_size() const noexcept { return ingested_size_; }

  private:
    unsigned int mac_len_{MAC_LEN};
    unsigned int c_rounds_{C_ROUNDS};
    unsigned int d_rounds_{D_ROUNDS};

    std::unique_ptr<OSSL_LIB_CTX, LibCtxRecycler> lib_ctx_{LibCtxs.empty() ? OSSL_LIB_CTX_new() : LibCtxs.acquire()};
    std::unique_ptr<EVP_MAC, MacDeleter> mac_{EVP_MAC_fetch(lib_ctx_.get(), "SIPHASH", nullptr)};
    std::unique_ptr<EVP_MAC_CTX, MacCtxDeleter> ctx_{EVP_MAC_CTX_new(mac_.get())};
    std::array<OSSL_PARAM, 4> params_{OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_SIZE, &mac_len_),
                                      OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_C_ROUNDS, &c_rounds_),
                                      OSSL_PARAM_construct_uint(OSSL_MAC_PARAM_D_ROUNDS, &d_rounds_),
                                      OSSL_PARAM_construct_end()};

    size_t ingested_size_{0};  // Number of bytes ingested
};

using SipHash24 = SipHash<8, 2, 4>;

}  // namespace znode::crypto