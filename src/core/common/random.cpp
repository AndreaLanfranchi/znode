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

#include "random.hpp"

#include <stdexcept>

namespace znode {

Bytes get_random_bytes(size_t size) noexcept {
    if (size == 0U) return {};
    Bytes bytes(size, 0);
    std::random_device rnd;
    std::mt19937 gen(rnd());
    std::uniform_int_distribution<uint16_t> dis(0, UINT8_MAX);
    for (auto& byte : bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    return bytes;
}

uint64_t randbits(uint8_t bits) noexcept {
    if (bits == 0U) return 0ULL;
    static THREAD_LOCAL uint64_t bit_buffer{0ULL};
    static THREAD_LOCAL uint8_t bit_buffer_size{0U};
    bits = std::min(bits, uint8_t(63U));
    if (bits > 32U) {
        return randomize<uint64_t>() >> (64U - bits);
    }
    if (bit_buffer_size < bits) {
        bit_buffer = randomize<uint64_t>();
        bit_buffer_size = static_cast<uint8_t>((bit_buffer)*size_t(CHAR_BIT));
    }
    uint64_t ret{bit_buffer & (UINT64_MAX >> (64U - bits))};
    bit_buffer >>= bits;
    bit_buffer_size -= bits;
    return ret;
}
}  // namespace znode
