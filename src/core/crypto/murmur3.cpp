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

#include "murmur3.hpp"

#include <bit>

#include <core/common/endian.hpp>

namespace znode::crypto {
namespace {

    ALWAYS_INLINE uint32_t fmix(uint32_t h) {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

}  // namespace
uint32_t Murmur3::Hash(const uint32_t seed, ByteView data) {
    uint32_t h1{seed};
    const uint32_t c1{0xcc9e2d51};
    const uint32_t c2{0x1b873593};

    const uint32_t data_len{static_cast<uint32_t>(data.size())};

    // Body
    // Consume data in chunks of 4 bytes (sizeof(uint32_t))
    while (data.length() >= sizeof(uint32_t)) {
        auto k1 = endian::load_little_u32(data.data());
        k1 *= c1;
        k1 = std::rotl(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = std::rotl(h1, 13);
        h1 *= 5;
        h1 += 0xe6546b64;
        data.remove_prefix(sizeof(uint32_t));
    }

    // Tail
    uint32_t k1{0};
    switch (data.size()) {
        case 3:
            k1 ^= static_cast<uint32_t>(data[2]) << 16;
            [[fallthrough]];
        case 2:
            k1 ^= static_cast<uint32_t>(data[1]) << 8;
            [[fallthrough]];
        case 1:
            k1 ^= static_cast<uint32_t>(data[0]);
            k1 *= c1;
            k1 = std::rotl(k1, 15);
            k1 *= c2;
            h1 ^= k1;
            [[fallthrough]];
        default:
            h1 ^= data_len;
            break;
    }

    // Final mix
    return fmix(h1);

}
}  // namespace znode::crypto
