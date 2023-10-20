/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "random.hpp"

namespace zenpp {

Bytes randomize_bytes(size_t size) {
    Bytes bytes(size, 0);
    std::random_device rnd;
    std::mt19937 gen(rnd());
    std::uniform_int_distribution<uint16_t> dis(0, UINT8_MAX);
    for (auto& byte : bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    return bytes;
}

}  // namespace zenpp