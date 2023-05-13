/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>
#include <iostream>

#include <openssl/evp.h>

#include <zen/core/common/assert.hpp>
#include <zen/core/common/base.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/object_pool.hpp>
#include <zen/core/common/secure_bytes.hpp>

namespace zen::crypto {

// static ZEN_THREAD_LOCAL ObjectPool<EVP_MD_CTX> DigestContexts;

//! \brief This class is templatized wrapper around OpenSSL's EVP Message Digests
template <StringLiteral T>
class MessageDigest {
  public:
    MessageDigest() : digest_{EVP_get_digestbyname(T.value)} {
        ZEN_ASSERT(digest_ != nullptr);
        digest_size_ = static_cast<size_t>(EVP_MD_size(digest_));
        block_size_ = static_cast<size_t>(EVP_MD_block_size(digest_));
        init();
    }

    //! \brief Instantiation with data initialization
    explicit MessageDigest(ByteView data) : MessageDigest() { update(data); }

    //! \brief Instantiation with data initialization
    explicit MessageDigest(std::string_view data) : MessageDigest() { update(data); }

    ~MessageDigest() { EVP_MD_CTX_free(digest_context_); }

    //! \brief Re-initialize the context pristine
    void init() noexcept {
        ingested_size_ = 0;
        init_context();
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
    int update(ByteView data) noexcept {
        ingested_size_ += data.size();
        return EVP_DigestUpdate(digest_context_, data.data(), data.size());
    }

    //! \brief Accumulates more data into the digest
    int update(std::string_view data) noexcept { return update(string_view_to_byte_view(data)); };

    //! \brief Finalizes the digest process and produces the actual digest
    //! \remarks After this instance has called finalize() once the instance itself cannot receive new updates unless
    //! it's recycled by init() or reset(). In case of any error the returned digest will be zero length
    [[nodiscard]] Bytes finalize() noexcept {
        Bytes ret(digest_size_, 0);
        if (EVP_DigestFinal_ex(digest_context_, ret.data(), nullptr) == 0) {
            ret.clear();
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
    const EVP_MD* digest_{nullptr};        // The digest function
    EVP_MD_CTX* digest_context_{nullptr};  // The digest context
    size_t digest_size_{0};                // The size in bytes of this digest
    size_t block_size_{0};                 // The size in bytes of an input block
    size_t ingested_size_{0};              // Number of bytes ingested

    //! \brief Obtain an instance of the context and initializes it
    void init_context() {
        if (!digest_context_) {
            digest_context_ = EVP_MD_CTX_new();
        }
        EVP_DigestInit_ex(digest_context_, digest_, nullptr);
    }
};

using Ripemd160 = MessageDigest<"RIPEMD160">;
using Sha1 = MessageDigest<"SHA1">;
using Sha256 = MessageDigest<"SHA256">;
using Sha512 = MessageDigest<"SHA512">;

}  // namespace zen::crypto
