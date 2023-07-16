/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/noncopyable.hpp>

#include <core/crypto/md.hpp>
#include <core/types/hash.hpp>

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

    static constexpr Bytes kEmptyHash() noexcept {
        // Known empty hash
        return {std::initializer_list<uint8_t>{0xb4, 0x72, 0xa2, 0x66, 0xd0, 0xbd, 0x89, 0xc1, 0x37, 0x06,
                                               0xa4, 0x13, 0x2c, 0xcf, 0xb1, 0x6f, 0x7c, 0x3b, 0x9f, 0xcb}};
    }

  private:
    Sha256 hasher_;
};

}  // namespace zen::crypto
