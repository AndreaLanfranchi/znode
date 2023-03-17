/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/crypto/hash256.hpp>
#include <zen/core/crypto/hasher_test.hpp>

namespace zen::crypto {

TEST_CASE("Bitcoin Hash256", "[crypto]") {
    // See https://github.com/nayuki/Bitcoin-Cryptography-Library/blob/master/cpp/Sha256Test.cpp
    const std::vector<std::string> inputs{
        "",                                                          // Test 1
        "abc",                                                       // Test 2
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",  // Test 3
    };

    std::vector<std::string> digests{
        "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",  // Test 1
        "58636c3ec08c12d55aedda056d602d5bcca72d8df6a69b519b72d32dc2428b4f",  // Test 2
        "af63952f8155cbb708b3b24997440992c95ebd5814fb843aac4d95687fe1ff0c",  // Test 3
    };

    // Above digests are taken as input for H256 which reverses the order bytes
    // hence we reverse the inputs too
    for (auto& digest : digests) {
        digest = hex::reverse_hex(digest);
    }

    Hash256 hasher;
    run_hasher_tests(hasher, inputs, digests);
}
}  // namespace zen::crypto
