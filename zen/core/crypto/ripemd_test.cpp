/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <random>

#include <catch2/catch.hpp>

#include <zen/core/crypto/hasher_test.hpp>
#include <zen/core/crypto/ripemd.hpp>

namespace zen::crypto {

TEST_CASE("Ripemd test vectors", "[crypto]") {
    static std::vector<std::string> inputs{
        "",                                                                  // Test 1
        "abc",                                                               // Test 2
        "message digest",                                                    // Test 3
        "secure hash algorithm",                                             // Test 4
        "RIPEMD160 is considered to be safe",                                // Test 5
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",          // Test 6
        "For this sample, this 63-byte string will be used as input data",   // Test 7
        "This is exactly 64 bytes long, not counting the terminating byte",  // Test 8
        std::string(1'000'000, 'a'),                                         // Test 9
    };

    static const std::vector<std::string> digests{
        "9c1185a5c5e9fc54612808977ee8f548b2258d31",  // Test 1
        "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc",  // Test 2
        "5d0689ef49d2fae572b881b123a85ffa21595f36",  // Test 3
        "20397528223b6a5f4cbc2808aba0464e645544f9",  // Test 4
        "a7d78608c7af8a8e728778e81576870734122b66",  // Test 5
        "12a053384a9c0c88e405a06c27dcf49ada62eb2b",  // Test 6
        "de90dbfee14b63fb5abf27c2ad4a82aaa5f27a11",  // Test 7
        "eda31d51d3a623b81e19eb02e24ff65d27d67b37",  // Test 8
        "52783243c1697bdbe16d37f97f68f08325dc1528",  // Test 9
    };

    Ripemd160 hasher;
    run_hasher_tests(hasher, inputs, digests);
}
}  // namespace zen::crypto
