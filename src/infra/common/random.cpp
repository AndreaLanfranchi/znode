/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
