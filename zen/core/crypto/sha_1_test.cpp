/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/crypto/hasher_test.hpp>
#include <zen/core/crypto/sha_1.hpp>

namespace zen::crypto {

TEST_CASE("Sha1 test vectors", "[crypto]") {
    static const std::vector<std::string> inputs{
        "",                                                                  // Test 1
        "abc",                                                               // Test 2
        "message digest",                                                    // Test 3
        "secure hash algorithm",                                             // Test 4
        "SHA1 is considered to be safe",                                     // Test 5
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",          // Test 6
        "For this sample, this 63-byte string will be used as input data",   // Test 7
        "This is exactly 64 bytes long, not counting the terminating byte",  // Test 8
        std::string(1'000'000, 'a'),                                         // Test 9
    };

    static const std::vector<std::string> digests{
        "da39a3ee5e6b4b0d3255bfef95601890afd80709",  // Test 1
        "a9993e364706816aba3e25717850c26c9cd0d89d",  // Test 2
        "c12252ceda8be8994d5fa0290a47231c1d16aae3",  // Test 3
        "d4d6d2f0ebe317513bbd8d967d89bac5819c2f60",  // Test 4
        "f2b6650569ad3a8720348dd6ea6c497dee3a842a",  // Test 5
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1",  // Test 6
        "4f0ea5cd0585a23d028abdc1a6684e5a8094dc49",  // Test 7
        "fb679f23e7d1ce053313e66e127ab1b444397057",  // Test 8
        "34aa973cd4c4daa4f61eeb2bdbad27316534016f",  // Test 9
    };

    Sha1 hasher;
    run_hasher_tests(hasher, inputs, digests);
}
}  // namespace zen::crypto
