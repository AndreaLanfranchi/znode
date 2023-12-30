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

#include <core/common/base.hpp>
#include <core/serialization/serializable.hpp>

namespace znode {

//! \brief A probabilistic filter which SPV clients provide so that can filter out
//! transactions that are not relevant to them This allows for significantly more efficient
//! transaction and block downloads.
//! \details Because bloom filters are probabilistic, a SPV node can increase the
//! false-positive rate making us send it transactions which aren't actually its,
//! allowing clients to trade more bandwidth for more privacy by obfuscating which keys are
//! controlled by them.
class BloomFilter : public ser::Serializable {
  public:
    static constexpr uint32_t kMaxFilterSize = 36000;  // Bytes
    static constexpr uint32_t kMaxHashFuncsCount = 50;

    enum class Flags : uint8_t {
        kNone = 0,
        kAll = 1,
        kP2PubKeyOnly = 2,
        kMask = 3
    };

    BloomFilter() = default;
    BloomFilter(size_t num_elements, double false_positive_rate, uint32_t tweaks, Flags flags);

    virtual ~BloomFilter() = default;

    //! \brief Inserts an element into the filter.
    //! \param[in] data The data to insert.
    //! \note Not thread safe.
    void insert(ByteView data);

    //! \brief Checks if an element matches in the filter.
    [[nodiscard]] bool contains(ByteView data) const;

    //! \brief Wether the filter size is within limits.
    //! \remarks Catches newly deserialized filters which are too large.
    [[nodiscard]] bool is_within_size_constraints() const;

  private:
    Bytes data_{};
    uint32_t hash_funcs_count_{0};
    uint32_t tweaks_{0};
    Flags flags_{Flags::kNone};

    uint32_t hash(uint32_t hash_num, ByteView data) const;

    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

//! \brief RollingBloomFilter is a probabilistic "keep track of most recently inserted" set.
//! \details Construct it with the number of items to keep track of, and a false-positive
//! rate. Unlike CBloomFilter, by default nTweak is set to a cryptographically secure random value for you. 
//! Similarly rather than clear() the method reset() is provided, which also changes 
//! nTweak to decrease the impact of false-positives.
//! contains(item) will always return true if item was one of the last N to 1.5*N insert()'ed 
//! but may also return true for items that were not inserted. 
//! * It needs around 1.8 bytes per element per factor 0.1 of false positive rate.
//! \verbatim
//! For example, if we want 1000 elements, we'd need: 
//! - ~1800 bytes for a false positive rate of 0.1
//! - ~3600 bytes for a false positive rate of 0.01
//! - ~5400 bytes for a false positive rate of 0.001
//! \endverbatim
//! If we make these simplifying assumptions:
//! - logFpRate / log(0.5) doesn't get rounded or clamped in the nHashFuncs calculation
//! - nElements is even, so that nEntriesPerGeneration == nElements / 2
//! Then we get a more accurate estimate for filter bytes:
//! 
//! 3/(log(256)*log(2)) * log(1/fpRate) * nElements
//! 
class RollingBloomFilter {

};
}  // namespace znode
