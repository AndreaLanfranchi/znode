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

#include <boost/noncopyable.hpp>

#include <core/crypto/md.hpp>

namespace znode::crypto {
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

    [[nodiscard]] size_t digest_size() const noexcept { return hasher_.digest_size(); }
    [[nodiscard]] size_t ingested_size() const noexcept { return ingested_size_; }

    static constexpr Bytes kEmptyHash() noexcept {
        // Known empty hash
        return {std::initializer_list<uint8_t>{0x5d, 0xf6, 0xe0, 0xe2, 0x76, 0x13, 0x59, 0xd3, 0x0a, 0x82, 0x75,
                                               0x05, 0x8e, 0x29, 0x9f, 0xcc, 0x03, 0x81, 0x53, 0x45, 0x45, 0xf5,
                                               0x5c, 0xf4, 0x3e, 0x41, 0x98, 0x3f, 0x5d, 0x4c, 0x94, 0x56}};
    }

  private:
    Sha256 hasher_;
    size_t ingested_size_{0};
};
}  // namespace znode::crypto
