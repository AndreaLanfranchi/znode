/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <benchmark/benchmark.h>

#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/misc.hpp>
#include <core/crypto/md.hpp>

namespace zenpp::crypto {

static constexpr size_t kMinInputSize{64};
static constexpr size_t kMaxInputSize{4_MiB};
static constexpr size_t kInputSizeMultiplier{8};

std::string random_alpha_string{get_random_alpha_string(kMaxInputSize)};

void bench_sha1(benchmark::State& state) {
    static crypto::Sha1 hasher;
    const ByteView data(byte_ptr_cast(random_alpha_string.data()), static_cast<size_t>(state.range()));
    for ([[maybe_unused]] auto _ : state) {
        hasher.init(data);
        auto hash{hasher.finalize()};
        benchmark::DoNotOptimize(hash);
    }
    state.SetBytesProcessed(state.range() * static_cast<int64_t>(state.iterations()));
}

void bench_sha256(benchmark::State& state) {
    static crypto::Sha256 hasher;
    const ByteView data(byte_ptr_cast(random_alpha_string.data()), static_cast<size_t>(state.range()));
    for ([[maybe_unused]] auto _ : state) {
        hasher.init(data);
        auto hash{hasher.finalize()};
        benchmark::DoNotOptimize(hash);
    }
    state.SetBytesProcessed(state.range() * static_cast<int64_t>(state.iterations()));
}

void bench_sha512(benchmark::State& state) {
    static crypto::Sha512 hasher;
    const ByteView data(byte_ptr_cast(random_alpha_string.data()), static_cast<size_t>(state.range()));
    for ([[maybe_unused]] auto _ : state) {
        hasher.init(data);
        auto hash{hasher.finalize()};
        benchmark::DoNotOptimize(hash);
    }
    state.SetBytesProcessed(state.range() * static_cast<int64_t>(state.iterations()));
}

BENCHMARK(bench_sha1)->RangeMultiplier(kInputSizeMultiplier)->Range(kMinInputSize, kMaxInputSize);
BENCHMARK(bench_sha256)->RangeMultiplier(kInputSizeMultiplier)->Range(kMinInputSize, kMaxInputSize);
BENCHMARK(bench_sha512)->RangeMultiplier(kInputSizeMultiplier)->Range(kMinInputSize, kMaxInputSize);

}  // namespace zenpp::crypto
