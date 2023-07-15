#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

if (BUILD_TESTS)
    add_subdirectory(test)      # The testing suites
    add_subdirectory(benchmark) # The benchmarking suites
endif ()

if(NOT BUILD_CORE_ONLY)

    message(CHECK_START "Looking for required packages ...")
    find_package(CLI11 REQUIRED)
    if(NOT TARGET CLI11::CLI11)
        message(FATAL_ERROR "CLI11::CLI11 not found")
    endif()
    message(CHECK_PASS "All required packages found")

    # The node executable itself
    add_executable(${PROJECT_NAME}d "node.cpp;common.hpp;common.cpp")
    target_link_libraries(${PROJECT_NAME}d PRIVATE $CACHE{BUILD_MAIN_COMPONENT} zen-buildinfo CLI11::CLI11 $<$<BOOL:${MSVC}>:Kernel32.lib>)
    target_include_directories($CACHE{BUILD_MAIN_COMPONENT} PRIVATE $CACHE{BUILD_3RDP_SRC_DIR}/indicators/include)

    # The db-tool executable
    add_executable(${PROJECT_NAME}-dbtool "toolbox.cpp;common.hpp;common.cpp")
    target_link_libraries(${PROJECT_NAME}-dbtool PRIVATE $CACHE{BUILD_MAIN_COMPONENT} zen-buildinfo CLI11::CLI11)
    target_include_directories(${PROJECT_NAME}-dbtool PRIVATE $CACHE{BUILD_3RDP_SRC_DIR}/indicators/include)

endif()
