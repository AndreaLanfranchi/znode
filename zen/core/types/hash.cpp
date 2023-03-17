/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

//#include <zen/core/common/assert.hpp>
//#include <zen/core/common/endian.hpp>
//#include <zen/core/crypto/jenkins.hpp>
//#include <zen/core/types/hash.hpp>

namespace zen {

// template <uint32_t BITS>
// Hash<BITS>::Hash(ByteView init) {
//     ZEN_ASSERT(init.length() <= kSize);
//
//     // Align to the right
//     const auto offset{init.size() - kSize};
//     std::memcpy(&bytes_[offset], init.data(), init.size());
}

// template <uint32_t BITS>
// Hash<BITS>::Hash(uint64_t value) {
//     ZEN_ASSERT(sizeof(value) <= kSize);
//     const auto offset{size() - sizeof(value)};
//     endian::store_big_u64(&bytes_[offset], value);
// }

// template <uint32_t BITS>
// tl::expected<Hash<BITS>, DecodingError> Hash<BITS>::from_hex(std::string_view input) noexcept {
//     const auto parsed_bytes{hex::decode(input)};
//     if (!parsed_bytes) return tl::unexpected(parsed_bytes.error());
//     return Hash<BITS>(ByteView(*parsed_bytes));
// }

// template <uint32_t BITS>
// std::string Hash<BITS>::to_hex(bool with_prefix) const noexcept {
//     return zen::hex::encode({&bytes_[0], kSize}, with_prefix);
// }

// template <uint32_t BITS>
// std::string Hash<BITS>::to_string() const noexcept {
//     return Hash<BITS>::to_hex(false);
// }

// template <uint32_t BITS>
// uint64_t Hash<BITS>::hash(const Hash<BITS>& _salt) const noexcept {
//     const uint32_t* source{static_cast<uint32_t*>(data())};
//     const uint32_t* salt{static_cast<uint32_t*>(_salt.data())};
//     uint64_t ret{crypto::Jenkins::Hash(source, size(), salt)};
//     return ret;
// }

//}  // namespace zen
