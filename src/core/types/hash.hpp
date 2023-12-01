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
#include <array>
#include <ranges>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/endian.hpp>
#include <core/crypto/jenkins.hpp>
#include <core/encoding/hex.hpp>
#include <core/serialization/serializable.hpp>

namespace znode {

//! \brief A Hash is a fixed size sequence of bytes
template <uint32_t BITS>
class Hash : public ser::Serializable {
  public:
    static_assert(BITS && (BITS & 7) == 0, "Must be a multiple of 8");
    enum : uint32_t {
        kSize = BITS / 8
    };

    using iterator_type = typename std::array<uint8_t, kSize>::iterator;
    using const_iterator_type = typename std::array<uint8_t, kSize>::const_iterator;

    Hash() = default;

    //! \brief Creates a Hash from given input
    //! \remarks If len of input exceeds kHashLength then input is disregarded otherwise input is left padded with
    //! zeroes
    explicit Hash(ByteView init) {
        if (init.size() > kSize) return;
        // Align to the right
        const auto offset{kSize - init.size()};
        std::memcpy(&bytes_[offset], init.data(), init.size());
    }

    //! \brief Converting constructor from unsigned integer value.
    //! \details This constructor assigns the value to the last 8 bytes [24:31] in big endian order
    explicit Hash(uint64_t value) {
        ASSERT(sizeof(value) <= kSize);
        const auto offset{size() - sizeof(value)};
        endian::store_big_u64(&bytes_[offset], value);
    }

    //! \brief Returns a hash loaded from a hex string
    //! \param input The hex string to de-hexify
    //! \param reverse If true, the bytes sequence is reversed after being de-hexified
    static outcome::result<Hash<BITS>> from_hex(std::string_view input, bool reverse = false) noexcept {
        auto parsed_bytes{enc::hex::decode(input)};
        if (!parsed_bytes) return parsed_bytes.error();
        if (reverse) [[unlikely]]
            std::ranges::reverse(parsed_bytes.value());
        return Hash<BITS>(ByteView(parsed_bytes.value()));
    }

    //! \brief Returns the hexadecimal representation of this hash
    //! \param reverse If true, the bytes sequence is reversed before being hexed
    //! \param with_prefix If true, the returned string will have the 0x prefix
    [[nodiscard]] std::string to_hex(bool reverse = false, bool with_prefix = false) const noexcept {
        if (reverse) [[unlikely]] {
            auto reversed{bytes_};
            std::ranges::reverse(reversed);
            return enc::hex::encode({&reversed[0], kSize}, with_prefix);
        }
        return enc::hex::encode({&bytes_[0], kSize}, with_prefix);
    }

    //! \brief An alias for to_hex with no prefix
    //! \param reverse If true, the bytes sequence is reversed before being hexed
    [[nodiscard]] std::string to_string(bool reverse = false) const noexcept { return to_hex(reverse); }

    //! \brief The size of a Hash
    static constexpr size_t size() { return kSize; }

    //! \brief Returns the hash of data an behalf of Jenkins lookup3
    [[nodiscard]] uint64_t hash(const Hash<BITS>& salt) const noexcept {
        const uint32_t* source{reinterpret_cast<const uint32_t*>(data())};
        const uint32_t* slt{reinterpret_cast<const uint32_t*>(salt.data())};
        uint64_t ret{crypto::Jenkins::Hash(source, Hash<BITS>::size() / sizeof(uint32_t), slt)};
        return ret;
    }

    //! \brief Returns the hash to its pristine state (i.e. all zeroes)
    void reset() { bytes_.fill(0); }

    [[nodiscard]] const uint8_t* data() const noexcept { return bytes_.data(); }

    iterator_type begin() noexcept { return bytes_.begin(); }

    iterator_type end() noexcept { return bytes_.end(); }

    const_iterator_type cbegin() { return bytes_.cbegin(); }

    const_iterator_type cend() { return bytes_.cend(); }

    std::strong_ordering operator<=>(const Hash<BITS>& other) const {
        auto result(std::memcmp(bytes_.data(), other.bytes_.data(), kSize));
        if (result < 0) return std::strong_ordering::less;
        if (result > 0) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    };

    bool operator==(const Hash<BITS>& other) const { return *this <=> other == 0; }
    bool operator!=(const Hash<BITS>& other) const { return *this <=> other != 0; }
    bool operator<(const Hash<BITS>& other) const { return *this <=> other < 0; }
    bool operator<=(const Hash<BITS>& other) const { return *this <=> other <= 0; }
    bool operator>(const Hash<BITS>& other) const { return *this <=> other > 0; }
    bool operator>=(const Hash<BITS>& other) const { return *this <=> other >= 0; }

    inline explicit operator bool() const noexcept {
        return std::ranges::any_of(bytes_, [](const auto& byte) { return byte > 0; });
    }

  private:
    alignas(uint32_t) std::array<uint8_t, kSize> bytes_{0};

    friend class ser::SDataStream;
    [[nodiscard]] outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override {
        return stream.bind(bytes_, action);
    }
};

using h160 = Hash<160>;
using h256 = Hash<256>;

}  // namespace znode
