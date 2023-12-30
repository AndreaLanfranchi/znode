/*
   Copyright 2012-2022 The Bitcoin Core developers
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

#include "bloom.hpp"

#include <magic_enum.hpp>

#include <core/crypto/murmur3.hpp>

namespace znode {
namespace {

    constexpr double kLn2Squared = 0.4804530139182014246671025263266649717305529515945455;
    constexpr double kLn2 = 0.6931471805599453094172321214581765680755001343602552;

}  // namespace

BloomFilter::BloomFilter(size_t num_elements, double false_positive_rate, uint32_t tweaks, Flags flags) {
    // The ideal size for a bloom filter with a given number of elements and false positive rate is:
    // * -n ln(p) / (ln(2)^2)
    // See: https://en.wikipedia.org/wiki/Bloom_filter#Probability_of_false_positives
    // 0.69 is approximately ln(2)^2.
    // We ignore parameters that would create a bloom filter larger than the protocol limits.
    uint32_t size = static_cast<uint32_t>(-1 / kLn2Squared * num_elements * std::log(false_positive_rate));
    data_.resize(std::min<uint32_t>(size, kMaxFilterSize * 8) / 8, 0);

    // The ideal number of hash functions is size / n ln(2) number of elements
    // Again we ignore parameters that would create a bloom filter larger than the protocol limits.
    uint32_t hash_funcs_count = static_cast<uint32_t>(data_.size() * 8 / num_elements * kLn2);
    hash_funcs_count_ = std::min<uint32_t>(hash_funcs_count, kMaxHashFuncsCount);

    tweaks_ = tweaks;
    flags_ = flags;
}

inline uint32_t BloomFilter::hash(uint32_t hash_num, ByteView data) const {
    // 0xFBA4C795 chosen as it guarantees a reasonable bit difference between hash_num values.
    return crypto::Murmur3::Hash(hash_num * 0xFBA4C795U + static_cast<uint32_t>(tweaks_), data) %
           static_cast<uint32_t>(data_.size() * 8);
}

void BloomFilter::insert(ByteView data) {
    if (data_.empty()) return;  // Avoid division by zero in hash()
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        uint32_t bit_pos = hash(i, data);
        data_[bit_pos >> 3] |= static_cast<uint8_t>(1U << (7U & bit_pos));
    }
}

bool BloomFilter::contains(ByteView data) const {
    if (data_.empty()) return false;  // Avoid division by zero in hash()
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        uint32_t bit_pos = hash(i, data);
        if (!(data_[bit_pos >> 3] & static_cast<uint8_t>(1U << (7U & bit_pos)))) return false;
    }
    return true;
}

bool BloomFilter::is_within_size_constraints() const {
    return (data_.size() <= kMaxFilterSize && hash_funcs_count_ <= kMaxHashFuncsCount);
}

outcome::result<void> BloomFilter::serialization(ser::SDataStream& stream, ser::Action action) {
    auto result{stream.bind(data_, action)};
    if (not result.has_error()) result = stream.bind(hash_funcs_count_, action);
    if (not result.has_error()) result = stream.bind(tweaks_, action);
    if (not result.has_error()) {
        auto flag_value{static_cast<uint8_t>(flags_)};
        result = stream.bind(flag_value, action);
        if (auto tmp{magic_enum::enum_cast<Flags>(flag_value)}; tmp.has_value()) {
            flags_ = *tmp;
        } else {
            result = ser::Error::kInvalidEnumValue;
        }
    }
    return result;
}
}  // namespace znode
