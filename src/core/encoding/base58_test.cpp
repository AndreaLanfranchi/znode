/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <catch2/catch.hpp>

#include <core/common/cast.hpp>
#include <core/encoding/base58.hpp>
#include <core/encoding/hex.hpp>

namespace zen::base58 {

// See https://github.com/status-im/nim-stew/blob/master/tests/test_base58.nim
// Checked they're the same as Zen old code base
const std::vector<std::pair</*hex*/ std::string, /*base58*/ std::string>> tests{
    {"", ""},
    {"61", "2g"},
    {"626262", "a3gV"},
    {"636363", "aPEr"},
    {"73696d706c792061206c6f6e6720737472696e67", "2cFupjhnEsSn59qHXstmK2ffpLv2"},
    {"00eb15231dfceb60925886b67d065299925915aeb172c06647", "1NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L"},
    {"516b6fcd0f", "ABnLTmg"},
    {"bf4f89001e670274dd", "3SEo3LWLoPntC"},
    {"572e4794", "3EFU7m"},
    {"ecac89cad93923c02321", "EJDM8drfXA6uyA"},
    {"10c8511e", "Rt5zm"},
    {"00000000000000000000", "1111111111"},
    {"000111d38e5fc9071ffcd20b4a763cc9ae4f252bb4e48fd66a835e252ada93ff480d6dd43dc62a641155a5",
     "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"},
    {"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353"
     "63738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c"
     "6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a"
     "3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9"
     "dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
     "1cWB5HCBdLjAuqGGReWE3R3CguuwSjw6RHn39s2yuDRTS5NsBgNiFpWgAnEx6VQi8csexkgYw3mdYrMHr8x9i7aEwP8kZ7vccXWqKDvGv3u1G"
     "xFKPuAkn8JCPPGDMf3vMMnbzm6Nh9zh1gcNsMvH3ZNLmP5fSG6DGbbi2tuwMWPthr4boWwCxf7ewSgNQeacyozhKDDQQ1qL5fQFUW52QKUZDZ"
     "5fw3KXNQJMcNTcaB723LchjeKun7MuGW5qyCBZYzA1KjofN1gYBV3NqyhQJ3Ns746GNuf9N2pQPmHz4xpnSrrfCvy6TVVz5d4PdrjeshsWQwp"
     "ZsZGzvbdAdN8MKV5QsBDY"}};

TEST_CASE("Base58 encoding", "[encoding]") {
    for (const auto& [input, expected_output] : tests) {
        const auto bytes{hex::decode(input)};
        REQUIRE(bytes);
        const auto output{base58::encode(*bytes)};
        REQUIRE(output);
        CHECK(*output == expected_output);
    }
}

TEST_CASE("Base58 decoding", "[encoding]") {
    for (const auto& [expected_output, input] : tests) {
        const auto output{base58::decode(input)};
        REQUIRE(output);
        const auto hexed{hex::encode(*output)};
        CHECK(hexed == expected_output);
    }
}

TEST_CASE("Base58 encode/decode with checksum", "[encoding]") {
    for (const auto& [input, not_checksummed_output] : tests) {
        std::ignore = not_checksummed_output;
        const auto input_bytes{hex::decode(input)};
        REQUIRE(input_bytes);
        const auto checksum_encoded{encode_check(*input_bytes)};
        REQUIRE(checksum_encoded);

        const auto checksum_decoded{decode_check(*checksum_encoded)};
        REQUIRE(checksum_decoded);

        const auto hexed_checksum_decoded{hex::encode(*checksum_decoded)};
        CHECK(hexed_checksum_decoded == input);
    }
}

}  // namespace zen::base58
