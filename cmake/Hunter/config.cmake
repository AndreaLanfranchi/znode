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
        OpenSSL
        VERSION 3.1.0
)

hunter_config(
        Boost
#        VERSION 1.83
#        URL "https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.gz"
#        SHA1 "eb5e17350b5ccd5926fd6bad9f09385c742a3352"
        VERSION 1.82
        URL "https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz"
        SHA1 "0e0d4bdac2628ebeff2c2a63b87514217832839d"
        CMAKE_ARGS BOOST_ASSERT_CONFIG=ON
)
