/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/crypto/hasher.hpp>

namespace zen::crypto {
//! \brief A wrapper around OpenSSL's SHA1 crypto functions
class Sha1 final : public Hasher {
  public:
    Sha1();
    ~Sha1() override;

    explicit Sha1(ByteView initial_data);
    explicit Sha1(std::string_view initial_data);

    void init() noexcept override;
    [[nodiscard]] Bytes finalize() noexcept override;
    [[nodiscard]] Bytes finalize_nopadding(bool compression) const noexcept;

  private:
    std::unique_ptr<SHA_CTX> ctx_{nullptr};
    static ZEN_THREAD_LOCAL ObjectPool<SHA_CTX> ctx_pool_;

    void init_context() override;
    void transform(const unsigned char* data) override;
};
}  // namespace zen::crypto
