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

#include <fastrange.h>

#include <magic_enum.hpp>

#include <core/common/random.hpp>
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

inline uint32_t BloomFilter::hash(uint32_t hash_num, ByteView data) const noexcept {
    // 0xFBA4C795 chosen as it guarantees a reasonable bit difference between hash_num values.
    return crypto::Murmur3::Hash(hash_num * 0xFBA4C795U + static_cast<uint32_t>(tweaks_), data) %
           static_cast<uint32_t>(data_.size() * 8);
}

void BloomFilter::insert(ByteView data) noexcept {
    if (data_.empty()) return;  // Avoid division by zero in hash()
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        uint32_t bit_pos = hash(i, data);
        data_[bit_pos >> 3] |= static_cast<uint8_t>(1U << (7U & bit_pos));
    }
}

bool BloomFilter::contains(ByteView data) const noexcept {
    if (data_.empty()) return false;  // Avoid division by zero in hash()
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        uint32_t bit_pos = hash(i, data);
        if (!(data_[bit_pos >> 3] & static_cast<uint8_t>(1U << (7U & bit_pos)))) return false;
    }
    return true;
}

bool BloomFilter::is_within_size_constraints() const noexcept {
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

RollingBloomFilter::RollingBloomFilter(const uint32_t num_elements, const double false_positive_rate) {
    const double log_false_positive_rate{std::log(false_positive_rate)};
    /* The optimal number of hash functions is log(fpRate) / log(0.5), but
     * restrict it to the range 1-50. */
    hash_funcs_count_ =
        std::max(1U, std::min(static_cast<uint32_t>(std::round(log_false_positive_rate / std::log(0.5))), 50U));

    /* In this rolling bloom filter, we'll store between 2 and 3 generations of nElements / 2 entries. */
    num_entries_per_generation_ = (num_elements + 1) / 2;
    const uint32_t max_elements_count = num_entries_per_generation_ * 3;

    /* The maximum fpRate = pow(1.0 - exp(-nHashFuncs * nMaxElements / nFilterBits), nHashFuncs)
     * =>          pow(fpRate, 1.0 / nHashFuncs) = 1.0 - exp(-nHashFuncs * nMaxElements / nFilterBits)
     * =>          1.0 - pow(fpRate, 1.0 / nHashFuncs) = exp(-nHashFuncs * nMaxElements / nFilterBits)
     * =>          log(1.0 - pow(fpRate, 1.0 / nHashFuncs)) = -nHashFuncs * nMaxElements / nFilterBits
     * =>          nFilterBits = -nHashFuncs * nMaxElements / log(1.0 - pow(fpRate, 1.0 / nHashFuncs))
     * =>          nFilterBits = -nHashFuncs * nMaxElements / log(1.0 - exp(logFpRate / nHashFuncs))
     */
    const uint32_t filter_bits_count = static_cast<uint32_t>(std::ceil(
        -1.0 * hash_funcs_count_ * max_elements_count / std::log(1.0 - std::exp(log_false_positive_rate / hash_funcs_count_))));

    data_.resize(static_cast<decltype(data_.size())>((filter_bits_count + 63) / 64) << 1, 0);
    reset();
}

void RollingBloomFilter::insert(ByteView key) noexcept {
    if (num_entries_this_generation_ == num_entries_per_generation_) {
        num_entries_this_generation_ = 0;
        if (++generation_id_ == 4U) {
            generation_id_ = 1U;
        }

        const uint64_t gen_mask_1{0 - static_cast<uint64_t>(generation_id_ & 1U)};
        const uint64_t gen_mask_2{0 - static_cast<uint64_t>(generation_id_ >> 1U)};

        // Wipe old entries that used this generation id
        for (decltype(data_.size()) p{0}; p < data_.size(); p += 2U) {
            const uint64_t p1{data_[p]};
            const uint64_t p2{data_[p + 1]};
            const uint64_t mask{(p1 ^ gen_mask_1) | (p2 ^ gen_mask_2)};
            data_[p] = p1 & mask;
            data_[p + 1] = p2 & mask;
        }
    }
    ++num_entries_this_generation_;
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        const uint32_t h{hash(i, key)};
        const uint32_t bit{h & 0x3fU};
        const auto not_bit{~(uint64_t(1) << bit)};
        const auto pos{static_cast<decltype(data_.size())>(fastrange32(h, static_cast<uint32_t>(data_.size())))};
        data_[pos & ~1U] = (data_[pos & ~1U] & not_bit) | (uint64_t{generation_id_ & 1U}) << bit;
        data_[pos | 1U] = (data_[pos | 1U] & not_bit) | (uint64_t{generation_id_ >> 1U}) << bit;
    }
}

bool RollingBloomFilter::contains(ByteView key) const noexcept {
    for (uint32_t i{0}; i < hash_funcs_count_; ++i) {
        const uint32_t h{hash(i, key)};
        const uint32_t bit{h & 0x3fU};
        const auto pos{static_cast<decltype(data_.size())>(fastrange32(h, static_cast<uint32_t>(data_.size())))};
        /* If the relevant bit is not set in either data[pos & ~1] or data[pos | 1], the filter does not contain vKey */
        if (!(((data_[pos & ~1U] | data_[pos | 1U]) >> bit) & 1)) {
            return false;
        }
    }
    return true;
}

void RollingBloomFilter::reset() noexcept {
    tweaks_ = randomize<uint32_t>();
    num_entries_this_generation_ = 0;
    generation_id_ = 1;
    std::fill(data_.begin(), data_.end(), 0);
}

inline uint32_t RollingBloomFilter::hash(uint32_t hash_num, ByteView data) const noexcept {
    return crypto::Murmur3::Hash(static_cast<uint32_t>(hash_num * 0xFBA4C795U) + tweaks_, data);
}
}  // namespace znode
