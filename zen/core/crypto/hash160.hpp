/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/noncopyable.hpp>

#include <zen/core/crypto/sha_2_256.hpp>

namespace zen::crypto {
//! \brief A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160)
class Hash160 : public Hasher {
  public:
    Hash160();
    ~Hash160() override = default;

    explicit Hash160(ByteView initial_data);
    explicit Hash160(std::string_view initial_data);

    void init() noexcept override;
    void init(ByteView initial_data) noexcept;

    void update(ByteView data) noexcept override;
    void update(std::string_view data) noexcept override;
    [[nodiscard]] Bytes finalize() noexcept override;

  private:
    Sha256 hasher;

    void init_context() override{/* Need to override from parent class*/};
    void transform(const unsigned char*) override{/* Need to override from parent class*/};
};
}  // namespace zen::crypto
