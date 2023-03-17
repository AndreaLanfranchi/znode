/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hasher_test.hpp>
#include <zen/core/crypto/hmac.hpp>

namespace zen::crypto {

TEST_CASE("Hmac test vectors", "[crypto]") {
    // See https://www.rfc-editor.org/rfc/rfc4231
    static const std::vector<std::pair<std::string, std::string>> inputs{
        {zen::hex::encode(Bytes(20, 0x0b)), zen::hex::encode(string_view_to_byte_view("Hi There"))},  // Test 1
        {zen::hex::encode(string_view_to_byte_view("Jefe")),
         zen::hex::encode(string_view_to_byte_view("what do ya want for nothing?"))},               // Test 2
        {std::string(40, 'a'), std::string(100, 'd')},                                              // Test 3
        {"0102030405060708090a0b0c0d0e0f10111213141516171819", zen::hex::encode(Bytes(50, 0xcd))},  // Test 4
        {zen::hex::encode(Bytes(20, 0x0c)),
         zen::hex::encode(string_view_to_byte_view("Test With Truncation"))},  // Test 5
        {std::string(262, 'a'),
         zen::hex::encode(string_view_to_byte_view("Test Using Larger Than Block-Size Key - Hash Key First"))},  // Test
                                                                                                                 // 6
        {std::string(262, 'a'),
         zen::hex::encode(string_view_to_byte_view(
             "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be "
             "hashed before being used by the HMAC algorithm."))},  // Test 7
    };

    SECTION("Hmac256") {
        static const std::vector<std::string> digests{
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",  // Test 1
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",  // Test 2
            "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",  // Test 3
            "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",  // Test 4
            "a3b6167473100ee06e0c796c2955552b",                                  // Test 5
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",  // Test 6
            "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",  // Test 7
        };

        Hmac256 hasher;
        run_hasher_tests(hasher, inputs, digests);
    }

    SECTION("Hmac512") {
        static const std::vector<std::string> digests{
            "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
            "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854",  // Test 1
            "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
            "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",  // Test 2
            "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
            "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",  // Test 3
            "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3db"
            "a91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd",  // Test 4
            "415fad6271580a531d4179bc891d87a6",                                  // Test 5
            "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
            "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598",  // Test 6
            "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944"
            "b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58",  // Test 7
        };

        Hmac512 hasher;
        run_hasher_tests(hasher, inputs, digests);
    }
}
}  // namespace zen::crypto
