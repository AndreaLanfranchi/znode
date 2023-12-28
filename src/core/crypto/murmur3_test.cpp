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

#include "murmur3.hpp"

#include <vector>

#include <catch2/catch.hpp>

#include <core/encoding/hex.hpp>

namespace znode::crypto {

TEST_CASE("Murmur3 Hash", "[crypto]") {
    struct test_case {
        std::string input;
        uint32_t seed;
        uint32_t expected;
    };

    const std::vector<test_case> test_cases{
        /*See https://gist.github.com/vladimirgamalyan/defb2482feefbf5c3ea25b14c557753b */
        {"", 0, 0},
        {"", 1, 0x514E28B7},
        {"", 0xffffffff, 0x81F16F39},
        {"0xffffffff", 0, 0x76293B50},
        {"0x21436587", 0, 0xF55B516B},
        {"0x21436587", 0x5082EDEE, 0x2362F9DE},
        {"0x214365", 0, 0x7E4A8634},
        {"0x2143", 0, 0xA0F7B07A},
        {"0x21", 0, 0x72661CF4},
        {"0x00000000", 0, 0x2362F9DE},
        {"0x000000", 0, 0x85F0B427},
        {"0x0000", 0, 0x30F4C306},
        {"0x00", 0, 0x514E28B7},
        {"", 0, 0},
        {"", 1, 0x514E28B7},
        {"", 0xffffffff, 0x81F16F39},
        {"aaaa", 0x9747b28c, 0x5A97808A},
        {"aaa", 0x9747b28c, 0x283E0130},
        {"aa", 0x9747b28c, 0x5D211726},
        {"a", 0x9747b28c, 0x7FA09EA6},
        {"abcd", 0x9747b28c, 0xF0478627},
        {"abc", 0x9747b28c, 0xC84A62DD},
        {"ab", 0x9747b28c, 0x74875592},
        {"Hello, world!", 0x9747b28c, 0x24884CBA},
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         0x9747b28c, 0x37405BDC},
        {"abc", 0, 0xB3DD93FA},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 0, 0xEE925B90},
        {"The quick brown fox jumps over the lazy dog", 0x9747b28c, 0x2FA826CD},
        /*
         * See Bitcoin's core hash_tests.cpp
         * The magic number 0xFBA4C795 comes from CBloomFilter::Hash()
         */
        {"", 0xFBA4C795, 0x6a396f08U},
        {"", 0xffffffff, 0x81f16f39U},
        {"0x00", 0x00000000, 0x514e28b7U},
        {"0x00", 0xFBA4C795, 0xea3f0b17U},
        {"0xff", 0x00000000, 0xfd6cf10dU},
        {"0x0011", 0x00000000, 0x16c6b7abU},
        {"0x001122", 0x00000000, 0x8eb51c3dU},
        {"0x00112233", 0x00000000, 0xb4471bf8U},
        {"0x0011223344", 0x00000000, 0xe2301fa8U},
        {"0x001122334455", 0x00000000, 0xfc2e4a15U},
        {"0x00112233445566", 0x00000000, 0xb074502cU},
        {"0x0011223344556677", 0x00000000, 0x8034d2a0U},
        {"0x001122334455667788", 0x00000000, 0xb4698defU},
    };

    for (const auto& test : test_cases) {
        INFO("input = " << test.input);
        uint32_t hash{0};
        if (test.input.starts_with("0x")) {
            auto data{enc::hex::decode(test.input)};
            REQUIRE(data.has_value());
            hash = Murmur3::Hash(test.seed, data.value());
        } else {
            hash = Murmur3::Hash(test.seed, test.input);
        }
        CHECK(hash == test.expected);
    }
}

}  // namespace znode::crypto
