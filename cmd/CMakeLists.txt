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

if (BUILD_TESTS)
    if (TARGET Catch2::Catch2) # Catch2 imported target defined
        add_subdirectory(test)      # The testing suites
    endif ()
    if (TARGET benchmark::benchmark) # Benchmark imported target defined
        add_subdirectory(benchmark) # The benchmarking suites
    endif ()
endif ()

if (NOT BUILD_CORE_ONLY)

    set(COMMON_SRC
            common.hpp
            common.cpp
            common/nat_validator.hpp
            common/nat_validator.cpp
            common/size_validator.hpp
            common/shutdown_signal.hpp
            common/shutdown_signal.cpp
    )

    # The node executable itself
    add_executable(${PROJECT_NAME} "node.cpp;${COMMON_SRC}")
    target_link_libraries(${PROJECT_NAME} PUBLIC third-party-includes PRIVATE Boost::thread Boost::timer Boost::chrono ${BUILD_NODE_COMPONENT} ${BUILDINFO_LIB} CLI11::CLI11 $<$<BOOL:${MSVC}>:Kernel32.lib>)
    target_include_directories(${PROJECT_NAME} PRIVATE ${BUILD_MAIN_SRC_DIR})


    # The db-tool executable
    add_executable(${PROJECT_NAME}-dbtool "toolbox.cpp;${COMMON_SRC}")
    target_link_libraries(${PROJECT_NAME}-dbtool PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} CLI11::CLI11)
    target_include_directories(${PROJECT_NAME}-dbtool PRIVATE ${BUILD_MAIN_SRC_DIR})

endif ()
