/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/common/cast.hpp>
#include <core/crypto/hash256.hpp>

namespace zenpp::crypto {

Hash256::Hash256(ByteView data) : hasher_(data), ingested_size_{data.size()} {}

Hash256::Hash256(std::string_view data) : hasher_(data), ingested_size_{data.size()} {}

void Hash256::init() noexcept {
    hasher_.init();
    ingested_size_ = 0;
}

void Hash256::init(ByteView data) noexcept {
    hasher_.init(data);
    ingested_size_ = data.size();
}

void Hash256::init(std::string_view data) noexcept {
    hasher_.init(data);
    ingested_size_ = data.size();
}

void Hash256::update(ByteView data) noexcept {
    hasher_.update(data);
    ingested_size_ += data.size();
}

void Hash256::update(std::string_view data) noexcept {
    hasher_.update(data);
    ingested_size_ += data.size();
}

Bytes Hash256::finalize() noexcept {
    if (ingested_size_ == 0U) return kEmptyHash();
    Bytes data{hasher_.finalize()};
    if (data.empty()) return data;  // Some error occurred
    hasher_.init(data);             // 2nd pass
    return hasher_.finalize();
}
}  // namespace zenpp::crypto
