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

#include "hash256.hpp"

#include <core/common/cast.hpp>

namespace znode::crypto {

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
}  // namespace znode::crypto
