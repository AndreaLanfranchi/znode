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

#include "hash160.hpp"

#include <core/common/cast.hpp>

namespace znode::crypto {

Hash160::Hash160(ByteView data) : hasher_(data) {}

Hash160::Hash160(std::string_view data) : hasher_(data) {}

void Hash160::init() noexcept { hasher_.init(); }

void Hash160::init(ByteView data) noexcept { hasher_.init(data); }

void Hash160::init(std::string_view data) noexcept { hasher_.init(data); }

void Hash160::update(ByteView data) noexcept { hasher_.update(data); }

void Hash160::update(std::string_view data) noexcept { hasher_.update(data); }

Bytes Hash160::finalize() noexcept {
    if (hasher_.ingested_size() == 0U) return kEmptyHash();
    Bytes data{hasher_.finalize()};
    if (data.empty()) return data;  // Some error occurred
    Ripemd160 outer(data);
    return outer.finalize();
}
}  // namespace znode::crypto
