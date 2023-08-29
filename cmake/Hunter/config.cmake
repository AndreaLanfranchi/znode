#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

include(hunter_cmake_args)

hunter_cmake_args(
        abseil
        CMAKE_ARGS
        ABSL_BUILD_TESTING=OFF
        ABSL_PROPAGATE_CXX_STD=ON
        ABSL_ENABLE_INSTALL=OFF
        ABSL_RUN_TESTS=OFF
)

hunter_cmake_args(
        benchmark
        CMAKE_ARGS BENCHMARK_ENABLE_WERROR=OFF
)

hunter_config(
        intx
        VERSION 0.10.0
        URL https://github.com/chfast/intx/archive/refs/tags/v0.10.0.tar.gz
        SHA1 3a6ebe0b1a36527b6ef291ee93a8e508371e5b77
)

hunter_config(
        OpenSSL
        VERSION 3.1.0
)
