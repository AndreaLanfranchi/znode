/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/noncopyable.hpp>

#include <zen/core/crypto/md.hpp>

namespace zen::crypto {
//! \brief A hasher class for Bitcoin's 256 bit hash (double Sha256)
class Hash256 : private boost::noncopyable {
  public:
    Hash256() = default;
    ~Hash256() = default;

    explicit Hash256(ByteView initial_data);
    explicit Hash256(std::string_view initial_data);

    void init() noexcept;
    void init(ByteView data) noexcept;
    void init(std::string_view data) noexcept;

    void update(ByteView data) noexcept;
    void update(std::string_view data) noexcept;
    [[nodiscard]] Bytes finalize() noexcept;

    [[nodiscard]] size_t digest_size() const noexcept { return hasher.digest_size(); }
    [[nodiscard]] size_t ingested_size() const noexcept { return ingested_size_; }

  private:
    Sha256 hasher;
    size_t ingested_size_{0};  // Number of bytes ingested
};
}  // namespace zen::crypto
