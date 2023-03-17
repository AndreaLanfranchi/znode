#[[
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

find_package(benchmark CONFIG REQUIRED)

# Zen Core Benchmarks
message(STATUS "Looking for benchmarks ... ${ZEN_MAIN_SRC_DIR}/core/*_benchmark.cpp")
file(GLOB_RECURSE ZEN_CORE_BENCHMARKS CONFIGURE_DEPENDS "${ZEN_MAIN_SRC_DIR}/*_benchmark.cpp")
list(LENGTH ZEN_CORE_BENCHMARKS ZEN_CORE_SOURCE_ITEMS)
if (NOT ZEN_CORE_SOURCE_ITEMS EQUAL 0)
    add_executable(core_benchmarks benchmark_test.cpp ${ZEN_CORE_BENCHMARKS})
    target_link_libraries(core_benchmarks zen_core benchmark::benchmark)
endif ()

if (MSVC)
    target_compile_options(core_benchmarks PRIVATE /EHa- /EHsc)
else ()
    target_compile_options(core_benchmarks PRIVATE -fno-exceptions)
endif ()

if (NOT ZEN_CORE_ONLY)

    # Zen Node Benchmarks
    message(STATUS "Looking for benchmarks ... ${ZEN_MAIN_SRC_DIR}/node/*_benchmark.cpp")
    file(GLOB_RECURSE ZEN_NODE_BENCHMARKS CONFIGURE_DEPENDS "${ZEN_MAIN_SRC_DIR}/node/*_benchmark.cpp")
    list(LENGTH ZEN_NODE_BENCHMARKS ZEN_NODE_SOURCE_ITEMS)
    if (NOT ZEN_NODE_SOURCE_ITEMS EQUAL 0)
        add_executable(node_benchmarks benchmark_test.cpp ${ZEN_NODE_BENCHMARKS})
        target_link_libraries(node_benchmarks zen_node benchmark::benchmark)
    endif ()

endif ()
