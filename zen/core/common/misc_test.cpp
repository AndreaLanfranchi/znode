/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/common/cast.hpp>
#include <zen/core/common/misc.hpp>

namespace zen {

TEST_CASE("Parse Human Bytes", "[misc]") {
    auto parsed = parse_human_bytes("");
    CHECK((parsed && *parsed == 0));

    parsed = parse_human_bytes("not a number");
    CHECK_FALSE(parsed);

    const std::vector<std::pair<std::string, uint64_t>> tests{
        {"128", 128},      // Indivisible bytes
        {"128B", 128},     //
        {"128.32", 128},   //
        {"128.32B", 128},  //
        {"180", 180},      //

        {"640KB", 640_KB},   // Base 10
        {"640 KB", 640_KB},  //
        {"750 MB", 750_MB},  //
        {"400GB", 400_GB},   //
        {"2TB", 2_TB},       //
        {".5TB", 500_GB},    //
        {"0.5 TB", 500_GB},  //

        {"640KiB", 640_KiB},   // Base 2
        {"640 KiB", 640_KiB},  //
        {"750 MiB", 750_MiB},  //
        {"400GiB", 400_GiB},   //
        {"2TiB", 2_TiB},       //
        {".5TiB", 512_GiB},    //
        {"0.5 TiB", 512_GiB}   //
    };

    for (const auto& [input, expected] : tests) {
        const auto value = parse_human_bytes(input);
        REQUIRE(value);
        CHECK(*value == expected);
    }
}

TEST_CASE("to_string_binary", "[misc]") {
    const std::vector<std::pair<uint64_t, std::string>> tests{
        {1_TB, "1.00 TB"},             //
        {1_TB + 512_GB, "1.51 TB"},    //
        {1_TB + 256_GB, "1.26 TB"},    //
        {128, "128 B"},                //
        {46_MB, "46.00 MB"},           //
        {46_MB + 256_KB, "46.26 MB"},  //
        {1_KB, "1.00 KB"}              //
    };
    for (const auto& [val, expected] : tests) {
        CHECK(to_human_bytes(val, /*binary=*/false) == expected);
    }
    const std::vector<std::pair<uint64_t, std::string>> binary_tests{
        {1_TiB, "1.00 TiB"},              //
        {1_TiB + 512_GiB, "1.50 TiB"},    //
        {1_TiB + 256_GiB, "1.25 TiB"},    //
        {128, "128 B"},                   //
        {46_MiB, "46.00 MiB"},            //
        {46_MiB + 256_KiB, "46.25 MiB"},  //
        {1_KiB, "1.00 KiB"}               //
    };

    for (const auto& [val, expected] : binary_tests) {
        CHECK(to_human_bytes(val, /*binary=*/true) == expected);
    }
}

TEST_CASE("abridge", "[misc]") {
    std::string input = "01234567890";
    std::string abridged = abridge(input, 50);
    CHECK(input == abridged);
    abridged = abridge(input, 3);
    CHECK(abridged == "012...");

    CHECK(abridge("", 0).empty());
    CHECK(abridge("0123", 0) == "0123");
}

}  // namespace zen
