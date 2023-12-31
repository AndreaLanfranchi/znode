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

#pragma once
#include <random>
#include <vector>

#include <catch2/catch.hpp>

#include <core/common/cast.hpp>
#include <core/crypto/md.hpp>
#include <core/encoding/hex.hpp>

namespace znode::crypto {
template <typename Hasher>
void run_hasher_tests(Hasher& hasher, const std::vector<std::string>& inputs, const std::vector<std::string>& digests) {
    REQUIRE(inputs.size() == digests.size());

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    for (size_t i{0}; i < inputs.size(); ++i) {
        hasher.init();
        auto input(string_view_to_byte_view(inputs[i]));
        auto input_size{input.size()};

        // Consume input in pieces to ensure partial updates don't break anything
        while (!input.empty()) {
            std::uniform_int_distribution<size_t> uni(1ULL, (input.size() / 2) + 1);
            const size_t chunk_size{uni(rng)};
            const auto input_chunk{input.substr(0, chunk_size)};
            hasher.update(input_chunk);
            input.remove_prefix(chunk_size);
        }

        const auto hash{hasher.finalize()};
        CHECK(hasher.ingested_size() == input_size);
        CHECK(hash.size() == hasher.digest_size());
        CHECK(enc::hex::encode(hash) == digests[i]);
    }
}

template <typename Hasher>
void run_hasher_tests(Hasher& hasher, const std::vector<std::pair<std::string, std::string>>& inputs,
                      const std::vector<std::string>& digests) {
    REQUIRE(inputs.size() == digests.size());

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    for (size_t i{0}; i < inputs.size(); ++i) {
        const auto initial_key{enc::hex::decode(inputs[i].first).value()};
        const auto input{enc::hex::decode(inputs[i].second).value()};
        ByteView input_view{input};

        hasher.init(initial_key);

        // Consume input in pieces to ensure partial updates don't break anything
        while (!input_view.empty()) {
            std::uniform_int_distribution<size_t> uni(1ULL, (input_view.size() / 2) + 1);
            const size_t chunk_size{uni(rng)};
            const auto input_chunk{input_view.substr(0, chunk_size)};
            hasher.update(input_chunk);
            input_view.remove_prefix(chunk_size);
        }

        const auto hash{hasher.finalize()};
        CHECK(hash.size() == hasher.digest_size());
        const auto hexed_hash{enc::hex::encode(hash)};
        if (digests[i].length() < hexed_hash.length()) {
            CHECK(hexed_hash.substr(0, digests[i].size()) == digests[i]);
        } else {
            CHECK(hexed_hash == digests[i]);
        }
    }
}
}  // namespace znode::crypto
