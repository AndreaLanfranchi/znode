/*
   Copyright 2012-2022 The Bitcoin Core developers
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

#include "bloom.hpp"

#include <catch2/catch.hpp>

#include <core/common/random.hpp>
#include <core/encoding/hex.hpp>

namespace znode {
TEST_CASE("Bloom Filter", "[bloom]") {
    SECTION("Without tweaks") {
        BloomFilter filter(3, 0.01, 0, BloomFilter::Flags::kAll);

        std::string input{"99108ad8ed9bb6274d3980bab5a85c048f0950c8"};
        auto input_data{enc::hex::decode(input).value()};
        CHECK_FALSE(filter.contains(input_data));

        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        input = "19108ad8ed9bb6274d3980bab5a85c048f0950c8";
        input_data = enc::hex::decode(input).value();
        CHECK_FALSE(filter.contains(input_data));

        input = "b5a2c786d9ef4658287ced5914b37a1b4aa32eee";
        input_data = enc::hex::decode(input).value();
        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        input = "b9300670b4c5366e95b2699e8b18bc75e5f729c5";
        input_data = enc::hex::decode(input).value();
        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        ser::SDataStream stream(ser::Scope::kNetwork, 0);
        CHECK_FALSE(filter.serialize(stream).has_error());
        CHECK(enc::hex::encode(stream.read().value(), true) == "0x03614e9b050000000000000001");

        input = "99108ad8ed9bb6274d3980bab5a85c048f0950c8";
        input_data = enc::hex::decode(input).value();
        CHECK(filter.contains(input_data));
    }

    SECTION("With tweaks") {
        BloomFilter filter(3, 0.01, 2147483649UL, BloomFilter::Flags::kAll);

        std::string input{"99108ad8ed9bb6274d3980bab5a85c048f0950c8"};
        auto input_data{enc::hex::decode(input).value()};
        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        input = "19108ad8ed9bb6274d3980bab5a85c048f0950c8";
        input_data = enc::hex::decode(input).value();
        CHECK_FALSE(filter.contains(input_data));

        input = "b5a2c786d9ef4658287ced5914b37a1b4aa32eee";
        input_data = enc::hex::decode(input).value();
        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        input = "b9300670b4c5366e95b2699e8b18bc75e5f729c5";
        input_data = enc::hex::decode(input).value();
        filter.insert(input_data);
        CHECK(filter.contains(input_data));

        ser::SDataStream stream(ser::Scope::kNetwork, 0);
        CHECK_FALSE(filter.serialize(stream).has_error());
        CHECK(enc::hex::encode(stream.read().value(), true) == "0x03ce4299050000000100008001");
    }
}

TEST_CASE("Rolling Bloom Filter", "[bloom]") {
    // Last 100 entries, 1% false positive
    RollingBloomFilter rfilter(100, 0.01);

    // Overfill
    std::vector<Bytes> unique_inputs;
    while (unique_inputs.size() < 400) {
        auto input_data{get_random_bytes(64)};
        if (std::any_of(unique_inputs.begin(), unique_inputs.end(),
                        [&input_data](const Bytes& value) { return value == input_data; }))
            continue;
        rfilter.insert(input_data);
        unique_inputs.push_back(input_data);
    }

    // Ensure last 100 inserted items are remembered
    for (int i = 0; i < 100; ++i) {
        CHECK(rfilter.contains(unique_inputs[unique_inputs.size() - 100 + i]));
    }

    rfilter.reset();
    CHECK_FALSE(rfilter.contains(unique_inputs.back()));

    // Now roll through data and make sure last 100 entries are always
    // remembered.
    for (decltype(unique_inputs.size()) i{0}; i < unique_inputs.size(); ++i) {
        if (i >= 100) {
            CHECK(rfilter.contains(unique_inputs[i - 100]));
        }
        rfilter.insert(unique_inputs[i]);
        CHECK(rfilter.contains(unique_inputs[i]));
    }

    // Insert 999 more random entries
    for (int i{0}; i < 999; ++i) {
        auto input_data{get_random_bytes(64)};
        rfilter.insert(input_data);
        CHECK(rfilter.contains(input_data));
    }

    uint32_t hit_count{0};
    for (const auto& input : unique_inputs) {
        if (rfilter.contains(input)) {
            ++hit_count;
        }
    }
    CHECK_NOFAIL(hit_count <= 5);  // It's an approximation

    // Now create a filter which has room for all our data
    RollingBloomFilter rfilter2(1'000, 0.01);
    // Insert all our data
    for (const auto& input : unique_inputs) {
        rfilter2.insert(input);
    }
    // Ensure all items are remembered
    for (const auto& input : unique_inputs) {
        CHECK(rfilter2.contains(input));
    }
}
}  // namespace znode
