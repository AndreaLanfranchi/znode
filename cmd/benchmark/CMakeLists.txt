#[[
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

# Core Benchmarks
message(CHECK_START "Looking for core benchmarks ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/core/*_benchmark.?pp")
list(LENGTH SOURCES SOURCE_ITEMS)
message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
if (NOT SOURCE_ITEMS EQUAL 0)
    set(CORE_BENCH_TARGET "${PROJECT_NAME}-core-benchmarks")
    add_executable(${CORE_BENCH_TARGET} benchmark_test.cpp ${SOURCES})
    target_link_libraries(${CORE_BENCH_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_CORE_COMPONENT} benchmark::benchmark)
    target_include_directories(${CORE_BENCH_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})

    # No exceptions here
    target_compile_definitions(${CORE_BENCH_TARGET} PRIVATE BOOST_NO_EXCEPTIONS)
    if (MSVC)
        target_compile_options(${CORE_BENCH_TARGET} PRIVATE /EHa- /EHsc)
    else ()
        target_compile_options(${CORE_BENCH_TARGET} PRIVATE -fno-exceptions)
    endif ()

endif ()

if (NOT BUILD_CORE_ONLY)

    # Infra Benchmarks
    message(CHECK_START "Looking for infra benchmarks ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/infra/*_benchmark.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        set(INFRA_BENCH_TARGET "${PROJECT_NAME}-node-benchmarks")
        add_executable(${INFRA_BENCH_TARGET} benchmark_test.cpp ${SOURCES})
        target_link_libraries(${INFRA_BENCH_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} benchmark::benchmark)
        target_include_directories(${INFRA_BENCH_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})
    endif ()

    # Node Benchmarks
    message(CHECK_START "Looking for node benchmarks ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/node/*_benchmark.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        set(NODE_BENCH_TARGET "${PROJECT_NAME}-node-benchmarks")
        add_executable(${NODE_BENCH_TARGET} benchmark_test.cpp ${SOURCES})
        target_link_libraries(${NODE_BENCH_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_INFRA_COMPONENT} ${BUILD_NODE_COMPONENT} benchmark::benchmark)
        target_include_directories(${NODE_BENCH_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})
    endif ()

endif ()
