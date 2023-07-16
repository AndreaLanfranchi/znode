/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/crypto/md_test.hpp>

namespace zenpp::crypto {

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
    CHECK(hasher.digest_name() == "SHA1");
    run_hasher_tests(hasher, inputs, digests);
}

TEST_CASE("Sha2 test vectors", "[crypto]") {
    static const std::vector<std::string> inputs{
        "",                                                          // Test 1
        "abc",                                                       // Test 2
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",  // Test 3
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrs"
        "tu",                                                                                // Test 4
        std::string(1'000'000, 'a'),                                                         // Test 5
        "message digest",                                                                    // Test 6
        "secure hash algorithm",                                                             // Test 7
        "SHAXXX is considered to be safe",                                                   // Test 8
        "For this sample, this 63-byte string will be used as input data",                   // Test 9
        "This is exactly 64 bytes long, not counting the terminating byte",                  // Test 10
        "As Bitcoin relies on 80 byte header hashes, we want to have an example for that.",  // Test 11
    };

    SECTION("Sha256") {
        static const std::vector<std::string> digests{
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",  // Test 1
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",  // Test 2
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",  // Test 3
            "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",  // Test 4
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",  // Test 5
            "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650",  // Test 6
            "f30ceb2bb2829e79e4ca9753d35a8ecc00262d164cc077080295381cbd643f0d",  // Test 7
            "5c9e2f1bab6dc3ef6008564cba573a15e18427d775f8abced5012847e5677697",  // Test 8
            "f08a78cbbaee082b052ae0708f32fa1e50c5c421aa772ba5dbb406a2ea6be342",  // Test 9
            "ab64eff7e88e2e46165e29f2bce41826bd4c7b3552f6b382a9e7d3af47c245f8",  // Test 10
            "7406e8de7d6e4fffc573daef05aefb8806e7790f55eab5576f31349743cca743",  // Test 11
        };

        Sha256 hasher;
        CHECK(hasher.digest_name() == "SHA256");
        run_hasher_tests(hasher, inputs, digests);
    }

    SECTION("Sha256 compress no padding") {
        Sha256 hasher;
        CHECK(hasher.digest_name() == "SHA256");
        Bytes input(hasher.block_size(), 0);

        // Input the exact number of bytes as Digests' block size
        hasher.update(input);
        Bytes digest{hasher.finalize(/*compress=*/true)};
        CHECK_FALSE(digest.empty());
        // Notice the order of bytes is reversed (uint256S - from legacy code - does it)
        CHECK(hex::encode(digest) == "da5698be17b9b46962335799779fbeca8ce5d491c0d26243bafef9ea1837a9d8");

        // Alter the input length : should return an empty digest which means error
        input.push_back(0);
        hasher.init(input);
        digest.assign(hasher.finalize(/*compress=*/true));
        CHECK(digest.empty());

        // Insert 'a', 'b', 'c', 'd' 16 times
        input.assign({
            'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd',
            'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd',
            'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd',
            'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd',
        });
        hasher.init(input);
        digest.assign(hasher.finalize(/*compress=*/true));
        CHECK_FALSE(digest.empty());
        CHECK(hex::encode(digest) == "867d9811862dbdab2f8fa343e3e841df7db2ded433172800b0369e8741ec70da");
    }

    SECTION("Sha512") {
        static const std::vector<std::string> digests{
            "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
            "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",  // Test 1
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
            "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",  // Test 2
            "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
            "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445",  // Test 3
            "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
            "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",  // Test 4
            "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
            "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b",  // Test 5
            "107dbf389d9e9f71a3a95f6c055b9251bc5268c2be16d6c13492ea45b0199f33"
            "09e16455ab1e96118e8a905d5597b72038ddb372a89826046de66687bb420e7c",  // Test 6
            "7746d91f3de30c68cec0dd693120a7e8b04d8073cb699bdce1a3f64127bca7a3"
            "d5db502e814bb63c063a7a5043b2df87c61133395f4ad1edca7fcf4b30c3236e",  // Test 7
            "d983dc4ecc83e20b51c26c5ca440e8882fed8433eb7d3575dcb8b9bb5b776002"
            "399415eb6141f2f71dbb41a9a46dfc8f392239d817f23eb340cc79e5ea1b37c7",  // Test 8
            "b3de4afbc516d2478fe9b518d063bda6c8dd65fc38402dd81d1eb7364e72fb6e"
            "6663cf6d2771c8f5a6da09601712fb3d2a36c6ffea3e28b0818b05b0a8660766",  // Test 9
            "70aefeaa0e7ac4f8fe17532d7185a289bee3b428d950c14fa8b713ca09814a38"
            "7d245870e007a80ad97c369d193e41701aa07f3221d15f0e65a1ff970cedf030",  // Test 10
            "fc3d7af1ca4abe7faeb4e171b283986a8f407ff3165c6ec5b6191d4c2c3c0d8b"
            "ddca857774e06448e7899b1c2ae1d19345d057289ebf3a319d4b5777fa5e8b58",  // Test 11
        };

        Sha512 hasher;
        CHECK(hasher.digest_name() == "SHA512");
        run_hasher_tests(hasher, inputs, digests);
    }
}

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

}  // namespace zenpp::crypto
