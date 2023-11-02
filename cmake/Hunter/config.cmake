#[[
   Copyright 2022 The Silkworm Authors
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
