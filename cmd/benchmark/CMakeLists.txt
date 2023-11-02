#[[
   Copyright 2023 The Znode Authors

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
