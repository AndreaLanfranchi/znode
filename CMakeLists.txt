#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

cmake_minimum_required(VERSION 3.16.2)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cable/.git)
    message(FATAL_ERROR "Git submodules not initialized, execute:\n  git submodule update --init --recursive")
endif()

get_directory_property(ZEN_HAS_PARENT PARENT_DIRECTORY)

if(NOT ZEN_HAS_PARENT)

    include(cmake/cable/bootstrap.cmake)
    include(CableBuildType)
    cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Release Debug)

    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain/cxx20.cmake CACHE FILEPATH "" FORCE)
    endif()

    include(cmake/cable/HunterGate.cmake)
    HunterGate(
            URL "https://github.com/cpp-pm/hunter/archive/v0.24.3.tar.gz"
            SHA1 "10738b59e539818a01090e64c2d09896247530c7"
            LOCAL
    )

endif(NOT ZEN_HAS_PARENT)

#[[
If you want to include Zen++ into your CMake project,
add the following lines before the project command of your root CMakeLists.txt:

-----------------------------------------------------------------------------------------------------
include(zen/cmake/cable/bootstrap.cmake)
include(zen/cmake/cable/HunterGate.cmake)
HunterGate(
  URL "https://github.com/cpp-pm/hunter/archive/v0.24.3.tar.gz"
  SHA1 "10738b59e539818a01090e64c2d09896247530c7"
  FILEPATH "${CMAKE_SOURCE_DIR}/zen/cmake/Hunter/config.cmake"
)

-----------------------------------------------------------------------------------------------------

(You can update the Hunter version, but keep the FILEPATH option.)
]]

project(zen)
set(PROJECT_VERSION 0.0.1-dev)

include(CableBuildInfo)

string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${PROJECT_VERSION})
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})
set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_3})

cable_add_buildinfo_library(PROJECT_NAME ${PROJECT_NAME})

option(ZEN_CORE_ONLY "Only build Zen Core" OFF)
option(ZEN_CLANG_COVERAGE "Clang instrumentation for code coverage reports" OFF)
option(ZEN_SANITIZE "Build instrumentation for sanitizers" OFF)
option(ZEN_TESTS "Build tests" ON)

get_filename_component(ZEN_MAIN_DIR . ABSOLUTE)
set(ZEN_MAIN_SRC_DIR "${ZEN_MAIN_DIR}/zen")

message(
        "\n----------------------------------------------------------------------------\n"
        "-- Cmake    ${CMAKE_VERSION}\n"
        "-- Compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
        "-- Build    ${CMAKE_BUILD_TYPE} ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}\n"
        "----------------------------------------------------------------------------\n"
        "-- ZEN_CORE_ONLY      Only Build Zen Core                      ${ZEN_CORE_ONLY}\n"
        "-- ZEN_CLANG_COVERAGE Clang instrumentation for code coverage  ${ZEN_CLANG_COVERAGE}\n"
        "-- ZEN_SANITIZE       Build instrumentation for sanitizers     ${ZEN_SANITIZE}\n"
        "-- ZEN_TESTS          Build unit / consensus tests             ${ZEN_TESTS}\n"
        "----------------------------------------------------------------------------\n"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain/compiler_settings.cmake)

# Include dependencies handled by Hunter package manager
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hunter/core_packages.cmake)
find_package(Catch2 CONFIG REQUIRED)  # Tests

if(NOT ZEN_CORE_ONLY)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hunter/node_packages.cmake)

    find_package(Boost CONFIG REQUIRED)   # The Boost library
    find_package(intx CONFIG REQUIRED)    # Big integers

    # Silence CMake policy warnings in submodules
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW) # project() command manages VERSION variables
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW) # Honor visibility properties for all target types

    # MDBX
    set(MDBX_ENABLE_TESTS OFF)
    set(MDBX_ALLOY_BUILD OFF)
    set(MDBX_BUILD_TIMESTAMP "unknown") # For reproducible builds
    add_subdirectory(third-party/libmdbx)
    target_include_directories(mdbx-static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/libmdbx)

endif(NOT ZEN_CORE_ONLY)

# Compiler enforcements on Zen
if(NOT MSVC)

    add_compile_options(-Werror -Wall -Wextra -pedantic)
    add_compile_options(-Wshadow -Wimplicit-fallthrough -Wsign-conversion)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>)
    add_compile_options(-Wno-missing-field-initializers -Wnon-virtual-dtor)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-attributes)

        # coroutines support
        if((CMAKE_CXX_STANDARD GREATER_EQUAL 20) AND (NOT ZEN_WASM_API))
            add_compile_options(-fcoroutines)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME MATCHES "Darwin")
        add_compile_definitions(_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
        add_compile_options(-Wthread-safety)
    endif()

else()

    add_compile_options(/permissive-)

    add_compile_options(/we4018)  # Equivalent to -Wsign-conversion
    add_compile_options(/we4245)  # Equivalent to -Wsign-conversion
    add_compile_options(/we4267)  # Equivalent to -Wsign-conversion
    add_compile_options(/we4389)  # Equivalent to -Wsign-conversion

    add_compile_options(/we6244)  # Equivalent to -Wshadow
    add_compile_options(/we6246)  # Equivalent to -Wshadow
    add_compile_options(/we26819) # Equivalent to -Wimplicit-fallthrough

endif()

add_subdirectory(zen) # Zen itself
add_subdirectory(cmd) # Zen executables
