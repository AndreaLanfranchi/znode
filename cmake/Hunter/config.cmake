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

#        VERSION 1.1.1t
#        URL https://github.com/openssl/openssl/archive/OpenSSL_1_1_1t.tar.gz
#        SHA1 34ea65451f7fc4625f31ba50f89b3fbea12f13f3

        VERSION 3.1.0
        #URL https://github.com/openssl/openssl/archive/openssl-3.1.0.tar.gz
        #SHA1 1adb0f773af645b9f54738301920e5c74360b76d
)
