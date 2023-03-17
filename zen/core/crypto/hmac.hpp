/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <boost/noncopyable.hpp>

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/sha_2_256.hpp>
#include <zen/core/crypto/sha_2_512.hpp>

namespace zen::crypto {

//! \brief Wrapper around Hash-based Message Authentication Code
//! \remarks Need implementation of SHA_xxx wrappers
template <class SHA2_SIZE>
class Hmac : private boost::noncopyable {
  public:
    Hmac() = default;

    explicit Hmac(const ByteView initial_data) { init(initial_data); };
    explicit Hmac(const std::string_view initial_data) { init(initial_data); };

    [[nodiscard]] constexpr size_t digest_size() const { return inner.digest_size(); };
    [[nodiscard]] constexpr size_t block_size() const { return inner.block_size(); };

    void init(const ByteView initial_data) {
        inner.init();
        outer.init();

        Bytes rkey;
        rkey.reserve(inner.block_size());
        if (initial_data.length() > inner.block_size()) {
            inner.update(initial_data);
            rkey.assign(inner.finalize());
            inner.init();  // Reset
        } else {
            rkey.assign(initial_data);
        }
        rkey.resize(inner.block_size(), 0);

        for (size_t i{0}; i < rkey.size(); ++i) {
            rkey[i] ^= 0x5c;
        }
        outer.update(rkey);

        for (size_t i{0}; i < rkey.size(); ++i) {
            rkey[i] ^= 0x5c ^ 0x36;
        }
        inner.update(rkey);
    };

    void init(const std::string_view initial_data) { init(string_view_to_byte_view(initial_data)); };

    void update(ByteView data) noexcept { inner.update(data); };
    void update(std::string_view data) noexcept { inner.update(data); };

    Bytes finalize() {
        outer.update(inner.finalize());
        return outer.finalize();
    };

  private:
    SHA2_SIZE inner{};
    SHA2_SIZE outer{};
};

using Hmac256 = Hmac<Sha256>;
using Hmac512 = Hmac<Sha512>;

}  // namespace zen::crypto