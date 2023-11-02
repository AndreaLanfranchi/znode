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

#include <vector>

#include <catch2/catch.hpp>

#include <core/crypto/hash256.hpp>
#include <core/crypto/md_test.hpp>

namespace znode::crypto {

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
        digest = enc::hex::reverse_hex(digest);
    }

    Hash256 hasher;
    run_hasher_tests(hasher, inputs, digests);
}
}  // namespace znode::crypto
