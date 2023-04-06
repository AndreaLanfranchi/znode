#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

if (ZEN_TESTS)
    add_subdirectory(test)      # The testing suites
    add_subdirectory(benchmark) # The benchmarking suites
endif ()

if(NOT ZEN_CORE_ONLY)

    find_package(absl REQUIRED)
    find_package(Boost REQUIRED)
    find_package(CLI11 REQUIRED)

    # The node itself
    add_executable(zend "node.cpp;common.hpp;common.cpp")
    target_link_libraries(zend PRIVATE zen_node zen-buildinfo CLI11::CLI11 $<$<BOOL:${MSVC}>:Kernel32.lib>)
    target_include_directories(zend PRIVATE ${ZEN_MAIN_DIR}/third-party/indicators/include)

endif()
