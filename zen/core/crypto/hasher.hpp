/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>

#include <boost/noncopyable.hpp>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

#include <zen/core/common/assert.hpp>
#include <zen/core/common/base.hpp>
#include <zen/core/common/object_pool.hpp>
#include <zen/core/common/secure_bytes.hpp>

namespace zen::crypto {

class Sha1;       // See sha_1.?pp
class Sha256;     // See sha_256.?pp
class Sha512;     // See sha_512.?pp
class Ripemd160;  // See ripemd.?pp

//! \brief A wrapper around OpenSSL's Hashing functions
class Hasher : private boost::noncopyable {
  public:
    Hasher(size_t _digest_size, size_t _block_size)
        : digest_size_{_digest_size}, block_size_{_block_size}, buffer_(_block_size, 0x0){};
    virtual ~Hasher() = default;

    [[nodiscard]] constexpr size_t digest_size() const { return digest_size_; };
    [[nodiscard]] constexpr size_t block_size() const { return block_size_; };

    virtual void init() noexcept = 0;
    void reset() noexcept { init(); }  // Alias
    virtual void update(ByteView data) noexcept;
    virtual void update(std::string_view data) noexcept;
    [[nodiscard]] virtual Bytes finalize() noexcept = 0;

  private:
    friend class Sha1;
    friend class Sha256;
    friend class Sha512;
    friend class Ripemd160;

    const size_t digest_size_;
    const size_t block_size_;
    SecureBytes buffer_;
    size_t buffer_offset_{0};
    size_t total_bytes_{0};

    virtual void init_context() = 0;
    virtual void transform(const unsigned char* data) = 0;
};

}  // namespace zen::crypto
