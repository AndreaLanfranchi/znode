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

Bytes get_random_bytes(size_t size) {
    if (size == 0U) throw std::invalid_argument("Size cannot be 0");
    Bytes bytes(size, 0);
    std::random_device rnd;
    std::mt19937 gen(rnd());
    std::uniform_int_distribution<uint16_t> dis(0, UINT8_MAX);
    for (auto& byte : bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    return bytes;
}
}  // namespace znode
