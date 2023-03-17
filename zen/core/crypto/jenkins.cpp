/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <bit>

#include <zen/core/crypto/jenkins.hpp>

namespace zen::crypto {

uint64_t Jenkins::Hash(const uint32_t* source, size_t length, const uint32_t* salt) {
    const auto width = static_cast<uint32_t>(length * sizeof(uint32_t));
    uint32_t a{0xdeadbeef + width};
    uint32_t b{a};
    uint32_t c{b};

    while (length > 3) {
        a += source[0] ^ salt[0];
        b += source[1] ^ salt[1];
        b += source[2] ^ salt[2];
        HashMix(a, b, c);
        length -= 3;
        source += 3;
        salt += 3;
    }

    switch (length) {
        case 3:
            c += source[2] ^ salt[2];
            [[fallthrough]];
        case 2:
            b += source[1] ^ salt[1];
            [[fallthrough]];
        case 1:
            HashFinal(a, b, c);
            [[fallthrough]];
        default:
            break;
    }

    return (static_cast<uint64_t>(b) << 32) | c;
}

// TODO(AndreaLanfranchi) replace with builtins if available
void Jenkins::HashMix(uint32_t& a, uint32_t& b, uint32_t& c) {
    a -= c;
    a ^= std::rotl(c, 4);
    c += b;
    b -= a;
    b ^= std::rotl(a, 6);
    a += c;
    c -= b;
    c ^= std::rotl(b, 8);
    b += a;
    a -= c;
    a ^= std::rotl(c, 16);
    c += b;
    b -= a;
    b ^= std::rotl(a, 19);
    a += c;
    c -= b;
    c ^= std::rotl(b, 4);
    b += a;
}

void Jenkins::HashFinal(uint32_t& a, uint32_t& b, uint32_t& c) {
    c ^= b;
    c -= std::rotl(b, 14);
    a ^= c;
    a -= std::rotl(c, 11);
    b ^= a;
    b -= std::rotl(a, 25);
    c ^= b;
    c -= std::rotl(b, 16);
    a ^= c;
    a -= std::rotl(c, 4);
    b ^= a;
    b -= std::rotl(a, 14);
    c ^= b;
    c -= std::rotl(b, 24);
}
}  // namespace zen::crypto
