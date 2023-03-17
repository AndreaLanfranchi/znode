/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <benchmark/benchmark.h>

#include <zen/core/common/base.hpp>
#include <zen/core/common/cast.hpp>
#include <zen/core/common/misc.hpp>
#include <zen/core/crypto/sha_1.hpp>
#include <zen/core/crypto/sha_2_256.hpp>
#include <zen/core/crypto/sha_2_256_old.hpp>
#include <zen/core/crypto/sha_2_512.hpp>

namespace zen {
static constexpr size_t kInputSize{4_KiB};

void bench_sha1(benchmark::State& state) {
    using namespace zen;
    int bytes_processed{0};
    int items_processed{0};
    zen::crypto::Sha1 hasher;
    for ([[maybe_unused]] auto _ : state) {
        hasher.init();
        std::string input{zen::get_random_alpha_string(kInputSize)};
        hasher.update(input);
        std::ignore = hasher.finalize();
        bytes_processed += static_cast<int>(input.size());
        ++items_processed;
        state.SetBytesProcessed(bytes_processed);
        state.SetItemsProcessed(items_processed);
    }
}

void bench_sha256(benchmark::State& state) {
    using namespace zen;
    int bytes_processed{0};
    int items_processed{0};
    zen::crypto::Sha256 hasher;
    for ([[maybe_unused]] auto _ : state) {
        hasher.init();
        std::string input{zen::get_random_alpha_string(kInputSize)};
        hasher.update(input);
        std::ignore = hasher.finalize();
        bytes_processed += static_cast<int>(input.size());
        ++items_processed;
        state.SetBytesProcessed(bytes_processed);
        state.SetItemsProcessed(items_processed);
    }
}

void bench_sha256_old(benchmark::State& state) {
    using namespace zen;
    int bytes_processed{0};
    int items_processed{0};
    zen::crypto::Sha256Old hasher;
    Bytes hash(hasher.OUTPUT_SIZE, '\0');
    for ([[maybe_unused]] auto _ : state) {
        hasher.Reset();
        std::string input{zen::get_random_alpha_string(kInputSize)};
        auto input_view{string_view_to_byte_view(input)};
        hasher.Write(input_view.data(), input.length());
        hasher.Finalize(hash.data());
        bytes_processed += static_cast<int>(input.size());
        ++items_processed;
        state.SetBytesProcessed(bytes_processed);
        state.SetItemsProcessed(items_processed);
    }
}

void bench_sha512(benchmark::State& state) {
    using namespace zen;
    int bytes_processed{0};
    int items_processed{0};
    zen::crypto::Sha512 hasher;
    for ([[maybe_unused]] auto _ : state) {
        hasher.init();
        std::string input{zen::get_random_alpha_string(kInputSize)};
        hasher.update(input);
        std::ignore = hasher.finalize();
        bytes_processed += static_cast<int>(input.size());
        ++items_processed;
        state.SetBytesProcessed(bytes_processed);
        state.SetItemsProcessed(items_processed);
    }
}

BENCHMARK(bench_sha1)->Arg(10'000);
BENCHMARK(bench_sha256)->Arg(10'000);
BENCHMARK(bench_sha256_old)->Arg(10'000);
BENCHMARK(bench_sha512)->Arg(10'000);

}  // namespace zen
