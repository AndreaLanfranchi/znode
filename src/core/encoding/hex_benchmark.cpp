/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <chrono>

#include <benchmark/benchmark.h>

#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/misc.hpp>
#include <core/encoding/hex.hpp>

namespace znode::enc::hex {

static constexpr size_t kInputSize{8_KiB};

void bench_hex(benchmark::State& state) {
    int bytes_processed{0};
    int items_processed{0};
    const auto input{get_random_alpha_string(kInputSize)};
    for ([[maybe_unused]] auto _ : state) {
        const auto hex_result{encode(string_view_to_byte_view(input))};
        benchmark::DoNotOptimize(hex_result.data());
        bytes_processed += static_cast<int>(hex_result.length());
        ++items_processed;
        state.SetBytesProcessed(bytes_processed);
        state.SetItemsProcessed(items_processed);
    }
}

BENCHMARK(bench_hex)->Arg(10'000);

}  // namespace znode::enc::hex