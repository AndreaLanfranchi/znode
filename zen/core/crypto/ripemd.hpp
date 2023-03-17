/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/crypto/hasher.hpp>

namespace zen::crypto {
//! \brief A wrapper around OpenSSL's RIPEMD160 crypto functions
class Ripemd160 : public Hasher {
  public:
    Ripemd160();
    ~Ripemd160() override;

    explicit Ripemd160(ByteView initial_data);
    explicit Ripemd160(std::string_view initial_data);

    void init() noexcept override;
    [[nodiscard]] Bytes finalize() noexcept override;

  private:
    std::unique_ptr<RIPEMD160_CTX> ctx_{nullptr};
    static ZEN_THREAD_LOCAL ObjectPool<RIPEMD160_CTX> ctx_pool_;

    void init_context() override;
    void transform(const unsigned char* data) override;
};

}  // namespace zen::crypto
