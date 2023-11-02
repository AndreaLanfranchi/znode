/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <memory>

#include <openssl/evp.h>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/endian.hpp>
#include <core/common/object_pool.hpp>

namespace znode::crypto {

//! \brief Explicit deleter for EVP_MD_CTXes
struct MDContextDeleter {
    constexpr MDContextDeleter() noexcept = default;
    void operator()(EVP_MD_CTX* ptr) const noexcept { EVP_MD_CTX_free(ptr); }
};

//! \brief A collection of recycle-able MD Contexts
static ObjectPool<EVP_MD_CTX, MDContextDeleter> MDContexts(/*thread_safe=*/true);

//! \brief Explicit deleter for EVP_MD_CTXes
struct MDContextRecycler {
    constexpr MDContextRecycler() noexcept = default;
    void operator()(EVP_MD_CTX* ptr) const noexcept { MDContexts.add(ptr); }
};

//! \brief This class is templatized wrapper around OpenSSL's EVP Message Digests
template <StringLiteral T>
class MessageDigest {
  public:
    MessageDigest()
        : digest_{EVP_get_digestbyname(T.value)},
          digest_context_{MDContexts.empty() ? EVP_MD_CTX_new() : MDContexts.acquire()} {
        ASSERT(digest_ != nullptr);
        ASSERT(digest_context_ != nullptr);
        ASSERT(EVP_DigestInit_ex(digest_context_.get(), digest_, nullptr) == 1);
        digest_size_ = static_cast<size_t>(EVP_MD_size(digest_));
        block_size_ = static_cast<size_t>(EVP_MD_block_size(digest_));
    }

    MessageDigest(MessageDigest& other) = delete;
    MessageDigest(MessageDigest&& other) = delete;
    MessageDigest& operator=(const MessageDigest& other) = delete;
    ~MessageDigest() = default;

    //! \brief Instantiation with data initialization
    explicit MessageDigest(ByteView data) : MessageDigest() { update(data); }

    //! \brief Instantiation with data initialization
    explicit MessageDigest(std::string_view data) : MessageDigest() { update(data); }

    //! \brief Re-initialize the context pristine
    void inline init() noexcept {
        ingested_size_ = 0;
        EVP_DigestInit_ex(digest_context_.get(), nullptr, nullptr);
    }

    //! \brief Re-initialize the context to provided initial data
    void init(ByteView data) noexcept {
        init();
        update(data);
    }

    //! \brief Re-initialize the context to provided initial data
    void init(std::string_view data) noexcept {
        init();
        update(data);
    }

    //! \brief Re-initialize the context to pristine
    //! \remarks Just an alias for init()
    void reset() noexcept { init(); }

    //! \brief Accumulates more data into the digest
    void update(ByteView data) noexcept {
        if (data.empty()) return;
        ingested_size_ += data.size();
        EVP_DigestUpdate(digest_context_.get(), data.data(), data.size());
    }

    //! \brief Accumulates more data into the digest
    void update(std::string_view data) noexcept { update(string_view_to_byte_view(data)); };

    //! \brief Finalizes the digest process and produces the actual digest
    //! \remarks After this instance has called finalize() once the instance itself cannot async_receive new updates
    //! unless it's recycled by init() or reset(). In case of any error the returned digest will be zero length
    [[nodiscard]] Bytes finalize(bool compress = false) noexcept {
        Bytes ret(digest_size_, 0);
        if (not compress) [[likely]] {
            if (EVP_DigestFinal_ex(digest_context_.get(), ret.data(), nullptr) == 0 /* zero is failure not success */) {
                ret.clear();
            }
        } else {
            // Only for Sha256 in Merkle tree composition
            if (digest_name() == "SHA256" and ingested_size_ == block_size_) {
                // See the structure of SHA256 which access is deprecated in OpenSSL 3.0.1
                // We need only first 8 integers
                // TODO: Seems OpenSSL 3.1.0 always returns nullptr
                const auto ctx{EVP_MD_CTX_get0_md_data(digest_context_.get())};
                if (ctx not_eq nullptr) {
                    const auto* ctx_data{reinterpret_cast<const uint32_t*>(ctx)};
                    for (size_t i{0}; i < 8; ++i) {
                        endian::store_big_u32(&ret[i << 2], ctx_data[i]);
                    }
                }
            }
        }
        return ret;
    }

    //! \brief Returns the digest name e.g. "SHA256"
    [[nodiscard]] std::string digest_name() const noexcept { return std::string(T.value); }

    //! \brief Returns the size (in bytes) of the final digest
    [[nodiscard]] size_t digest_size() const noexcept { return digest_size_; }

    //! \brief Returns the size (in bytes) of an input block
    [[nodiscard]] size_t block_size() const noexcept { return block_size_; }

    //! \brief Returns the number of bytes already digested
    [[nodiscard]] size_t ingested_size() const noexcept { return ingested_size_; }

  private:
    const EVP_MD* digest_{nullptr};                                           // The digest function
    std::unique_ptr<EVP_MD_CTX, MDContextRecycler> digest_context_{nullptr};  // The digest context
    size_t digest_size_{0};                                                   // The size in bytes of this digest
    size_t block_size_{0};                                                    // The size in bytes of an input block
    size_t ingested_size_{0};                                                 // Number of bytes ingested
};

using Ripemd160 = MessageDigest<"RIPEMD160">;
using Sha1 = MessageDigest<"SHA1">;
using Sha256 = MessageDigest<"SHA256">;
using Sha512 = MessageDigest<"SHA512">;

}  // namespace znode::crypto
