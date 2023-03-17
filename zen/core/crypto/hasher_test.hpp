/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <random>
#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/common/cast.hpp>
#include <zen/core/encoding/hex.hpp>

namespace zen::crypto {

template <typename Hasher>
void run_hasher_tests(Hasher& hasher, const std::vector<std::string>& inputs, const std::vector<std::string>& digests) {
    REQUIRE(inputs.size() == digests.size());

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    for (size_t i{0}; i < inputs.size(); ++i) {
        hasher.init();
        auto input(string_view_to_byte_view(inputs[i]));

        // Consume input in pieces to ensure partial updates don't break anything
        while (!input.empty()) {
            std::uniform_int_distribution<size_t> uni(1ULL, (input.size() / 2) + 1);
            const size_t chunk_size{uni(rng)};
            const auto input_chunk{input.substr(0, chunk_size)};
            hasher.update(input_chunk);
            input.remove_prefix(chunk_size);
        }

        const auto hash{hasher.finalize()};
        CHECK(hash.size() == hasher.digest_size());
        CHECK(zen::hex::encode(hash) == digests[i]);
    }
}

template <typename Hasher>
void run_hasher_tests(Hasher& hasher, const std::vector<std::pair<std::string, std::string>>& inputs,
                      const std::vector<std::string>& digests) {
    REQUIRE(inputs.size() == digests.size());

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    for (size_t i{0}; i < inputs.size(); ++i) {
        const auto initial_key{*zen::hex::decode(inputs[i].first)};
        const auto input{*zen::hex::decode(inputs[i].second)};
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
        const auto hexed_hash{zen::hex::encode(hash)};
        if (digests[i].length() < hexed_hash.length()) {
            CHECK(hexed_hash.substr(0, digests[i].size()) == digests[i]);
        } else {
            CHECK(hexed_hash == digests[i]);
        }
    }
}
}  // namespace zen::crypto
