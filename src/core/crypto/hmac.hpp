/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once
#include <ranges>

#include <boost/noncopyable.hpp>

#include <core/common/cast.hpp>
#include <core/crypto/md.hpp>

namespace znode::crypto {

//! \brief Wrapper around Hash-based Message Authentication Code
//! \remarks Need implementation of SHA_xxx wrappers
template <class DIGEST>
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

        std::ranges::for_each(rkey, [](auto& b) { b ^= 0x5c; });
        outer.update(rkey);

        std::ranges::for_each(rkey, [](auto& b) { b ^= 0x5c ^ 0x36; });
        inner.update(rkey);
    };

    void init(const std::string_view initial_data) { init(string_view_to_byte_view(initial_data)); };

    void update(ByteView data) noexcept { inner.update(data); };
    void update(std::string_view data) noexcept { inner.update(data); };

    Bytes finalize() {
        Bytes tmp{inner.finalize()};
        if (tmp.empty()) return tmp;
        outer.update(tmp);
        return outer.finalize();
    };

  private:
    DIGEST inner{};
    DIGEST outer{};
};

using Hmac256 = Hmac<Sha256>;
using Hmac512 = Hmac<Sha512>;

}  // namespace znode::crypto
