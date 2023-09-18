#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

if (BUILD_TESTS)
    if (TARGET Catch2::Catch2) # Catch2 imported target defined
        add_subdirectory(test)      # The testing suites
    endif ()
    if (TARGET benchmark::benchmark) # Benchmark imported target defined
        add_subdirectory(benchmark) # The benchmarking suites
    endif ()
endif ()

if (NOT BUILD_CORE_ONLY)

    # The node executable itself
    add_executable(${PROJECT_NAME} "node.cpp;common.hpp;common.cpp")
    target_link_libraries(${PROJECT_NAME} PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} ${BUILDINFO_LIB} CLI11::CLI11 $<$<BOOL:${MSVC}>:Kernel32.lib>)
    target_include_directories(${PROJECT_NAME} PRIVATE ${BUILD_MAIN_SRC_DIR})


    # The db-tool executable
    add_executable(${PROJECT_NAME}-dbtool "toolbox.cpp;common.hpp;common.cpp")
    target_link_libraries(${PROJECT_NAME}-dbtool PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} CLI11::CLI11)
    target_include_directories(${PROJECT_NAME}-dbtool PRIVATE ${BUILD_MAIN_SRC_DIR})

endif ()
