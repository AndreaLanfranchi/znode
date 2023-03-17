/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>
#include <ranges>

#include <zen/core/common/assert.hpp>
#include <zen/core/common/base.hpp>
#include <zen/core/common/endian.hpp>
#include <zen/core/crypto/jenkins.hpp>
#include <zen/core/encoding/hex.hpp>

namespace zen {
template <uint32_t BITS>
class Hash {
  public:
    static_assert(BITS >= 8 && BITS % 8 == 0, "Must be a multiple of 8");
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
        ZEN_ASSERT(sizeof(value) <= kSize);
        const auto offset{size() - sizeof(value)};
        endian::store_big_u64(&bytes_[offset], value);
    }

    //! \brief Returns a hash loaded from a hex string
    static tl::expected<Hash<BITS>, DecodingError> from_hex(std::string_view input) noexcept {
        const auto parsed_bytes{hex::decode(input)};
        if (!parsed_bytes) return tl::unexpected(parsed_bytes.error());
        return Hash<BITS>(ByteView(*parsed_bytes));
    }

    //! \brief Returns the hexadecimal representation of this hash
    [[nodiscard]] std::string to_hex(bool with_prefix = false) const noexcept {
        return hex::encode({&bytes_[0], kSize}, with_prefix);
    }

    //! \brief An alias for to_hex with no prefix
    [[nodiscard]] std::string to_string() const noexcept { return to_hex(false); }

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
    void reset() { memset(&bytes_, 0, kSize); }

    [[nodiscard]] const uint8_t* data() const noexcept { return bytes_.data(); }

    iterator_type begin() noexcept { return bytes_.begin(); }

    iterator_type end() noexcept { return bytes_.end(); }

    const_iterator_type cbegin() { return bytes_.cbegin(); }

    const_iterator_type cend() { return bytes_.cend(); }

    auto operator<=>(const Hash&) const = default;

    inline explicit operator bool() const noexcept {
        return std::ranges::find_if_not(bytes_, [](const uint8_t b) { return b != 0; }) == bytes_.end();
    }

  private:
    alignas(uint32_t) std::array<uint8_t, kSize> bytes_{0};
};

using h160 = Hash<160>;
using h256 = Hash<256>;

}  // namespace zen
