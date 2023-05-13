/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/noncopyable.hpp>

#include <zen/core/crypto/md.hpp>

namespace zen::crypto {
//! \brief A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160)
class Hash160 : private boost::noncopyable {
  public:
    Hash160() = default;
    ~Hash160() = default;

    explicit Hash160(ByteView data);
    explicit Hash160(std::string_view data);

    void init() noexcept;
    void init(ByteView data) noexcept;
    void init(std::string_view data) noexcept;

    void update(ByteView data) noexcept;
    void update(std::string_view data) noexcept;
    [[nodiscard]] Bytes finalize() noexcept;

  private:
    Sha256 inner_;
};

}  // namespace zen::crypto
