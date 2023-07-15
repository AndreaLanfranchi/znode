#[[
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

# Core Benchmarks
message(CHECK_START "Looking for core benchmarks ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "$CACHE{BUILD_MAIN_SRC_DIR}/core/*_benchmark.?pp")
list(LENGTH SOURCES SOURCE_ITEMS)
message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
if (NOT SOURCE_ITEMS EQUAL 0)
    add_executable(core_benchmarks benchmark_test.cpp ${SOURCES})
    target_link_libraries(core_benchmarks PRIVATE $CACHE{BUILD_CORE_COMPONENT} benchmark::benchmark)

    # No exceptions here
    target_compile_definitions(core_benchmarks PRIVATE BOOST_NO_EXCEPTIONS)
    if (MSVC)
        target_compile_options(core_benchmarks PRIVATE /EHa- /EHsc)
    else ()
        target_compile_options(core_benchmarks PRIVATE -fno-exceptions)
    endif ()

endif ()

if (NOT BUILD_CORE_ONLY)

    # Main app Benchmarks
    message(CHECK_START "Looking for main benchmarks ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "$CACHE{BUILD_MAIN_SRC_DIR}/main/*_benchmark.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        add_executable(main_benchmarks benchmark_test.cpp ${SOURCES})
        target_link_libraries(main_benchmarks $CACHE{BUILD_MAIN_COMPONENT} benchmark::benchmark)
    endif ()

endif ()
