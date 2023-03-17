/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/crypto/hasher.hpp>

namespace zen::crypto {
//! \brief A wrapper around OpenSSL's SHA512 crypto functions
class Sha512 : public Hasher {
  public:
    Sha512();
    ~Sha512() override;

    explicit Sha512(ByteView initial_data);
    explicit Sha512(std::string_view initial_data);

    void init() noexcept override;
    [[nodiscard]] Bytes finalize() noexcept override;
    [[nodiscard]] Bytes finalize_nopadding(bool compression) const noexcept;

  private:
    std::unique_ptr<SHA512_CTX> ctx_{nullptr};
    static ZEN_THREAD_LOCAL ObjectPool<SHA512_CTX> ctx_pool_;

    void init_context() override;
    void transform(const unsigned char* data) override;
};
}  // namespace zen::crypto
