#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

cmake_minimum_required(VERSION 3.20)

find_package(Git QUIET)
if (NOT GIT_FOUND)
    message(FATAL_ERROR "Git not found, please install it")
endif ()

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cable/.git)
    message(FATAL_ERROR "Git submodules not initialized, execute:\n$ git submodule update --init --recursive")
endif ()

if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_HOME_DIRECTORY}")
    message(FATAL_ERROR "In-source builds are not allowed. Create a separate directory (eg build/) for build files.")
endif ()

get_directory_property(PROJECT_HAS_PARENT PARENT_DIRECTORY)

if (NOT PROJECT_HAS_PARENT)

    include(cmake/cable/bootstrap.cmake)
    include(CableBuildType)
    cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Debug Release RelWithDebInfo)

    if (NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain/cxx20.cmake CACHE FILEPATH "" FORCE)
    endif ()

    include(cmake/HunterGate.cmake)
    HunterGate(
            URL "https://github.com/cpp-pm/hunter/archive/v0.24.18.tar.gz"
            SHA1 "1292e4d661e1770d6d6ca08c12c07cf34a0bf718"
            LOCAL
    )

endif (NOT PROJECT_HAS_PARENT)

#[[
If you want to include this project into your CMake project,
add the following lines before the project command of your root CMakeLists.txt.
Be sure to replace the tag <project-dir> with the name of the directory
where you have cloned this project.

-----------------------------------------------------------------------------------------------------
include(<project-dir>/cmake/cable/bootstrap.cmake)
include(<project-dir>/cmake/HunterGate.cmake)
HunterGate(
  URL "https://github.com/cpp-pm/hunter/archive/v0.24.18.tar.gz"
  SHA1 "1292e4d661e1770d6d6ca08c12c07cf34a0bf718"
  FILEPATH "${CMAKE_SOURCE_DIR}/<project-dir>/cmake/Hunter/config.cmake"
)

-----------------------------------------------------------------------------------------------------

(You can update the Hunter version, but keep the FILEPATH option.)
]]

project(zenpp)

# Execute git command to get the latest tag
execute_process(
        COMMAND git describe --abbrev=0 --tags
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE GIT_ERROR
        RESULT_VARIABLE GIT_RESULT
)

# Check if the git command was successful
if (GIT_RESULT EQUAL 0)
    # Parse the git tag to extract version information
    string(REGEX REPLACE "^v(.*)" "\\1" PROJECT_VERSION "${GIT_TAG}")

    # Print the extracted version
    message("Project Version: ${PROJECT_VERSION}")
else ()
    # No tags found or an error occurred
    if (GIT_ERROR)
        message(STATUS "No tags found in the repository: ${GIT_ERROR}")
    else ()
        message(FATAL_ERROR "Error executing git command: ${GIT_RESULT}")
    endif ()

    # Set a default version
    set(PROJECT_VERSION "0.1.0")
    message("Using default version: ${PROJECT_VERSION}")
endif ()

include(CableBuildInfo)

cable_add_buildinfo_library(PROJECT_NAME ${PROJECT_NAME})
if (NOT TARGET ${PROJECT_NAME}-buildinfo)
    message(FATAL_ERROR "Build info library not found")
else ()
    set(BUILDINFO_LIB ${PROJECT_NAME}-buildinfo)
endif ()

option(BUILD_CORE_ONLY "Only build ${PROJECT_NAME} Core" OFF)
option(BUILD_CLANG_COVERAGE "Clang instrumentation for code coverage reports" OFF)
option(BUILD_SANITIZE "Build instrumentation for sanitizers" OFF)
option(BUILD_TESTS "Build tests" ON)

get_filename_component(PROJECT_MAIN_DIR . ABSOLUTE)
set(BUILD_MAIN_SRC_DIR "${PROJECT_MAIN_DIR}/src")
set(BUILD_CORE_COMPONENT "${PROJECT_NAME}-core")
set(BUILD_NODE_COMPONENT "${PROJECT_NAME}-node")

message(
        "\n----------------------------------------------------------------------------\n"
        "-- Project  ${PROJECT_NAME} v${PROJECT_VERSION}\n"
        "-- Source   ${BUILD_MAIN_SRC_DIR}\n"
        "-- Cmake    ${CMAKE_VERSION}\n"
        "-- Compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
        "-- Build    ${CMAKE_BUILD_TYPE} ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}\n"
        "-- Host     ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}\n"
        "----------------------------------------------------------------------------\n"
        "-- BUILD_CORE_ONLY      Only Build Core components               ${BUILD_CORE_ONLY}\n"
        "-- BUILD_CLANG_COVERAGE Clang instrumentation for code coverage  ${BUILD_CLANG_COVERAGE}\n"
        "-- BUILD_SANITIZE       Build instrumentation for sanitizers     ${BUILD_SANITIZE}\n"
        "-- BUILD_TESTS          Build unit / consensus tests             ${BUILD_TESTS}\n"
        "----------------------------------------------------------------------------\n"
)

include(${PROJECT_MAIN_DIR}/cmake/toolchain/compiler_settings.cmake)
include(${PROJECT_MAIN_DIR}/cmake/Hunter/core_packages.cmake)

message(CHECK_START "Looking for required packages ...")
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
find_package(intx CONFIG REQUIRED)

if (NOT TARGET intx::intx)
    message(FATAL_ERROR "Mandatory package intx::intx not found")
elseif (NOT TARGET Microsoft.GSL::GSL)
    message(FATAL_ERROR "Mandatory package Microsoft.GSL::GSL not found")
elseif (NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "Mandatory package nlohmann_json::nlohmann_json not found")
elseif (NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR "Mandatory package OpenSSL::Crypto not found but OPENSSL_FOUND==${OPENSSL_FOUND} and OPENSSL_CRYPTO_LIBRARY==${OPENSSL_CRYPTO_LIBRARY}")
endif ()

if (BUILD_TESTS)
    find_package(Catch2 CONFIG REQUIRED)    # Tests
    find_package(benchmark CONFIG REQUIRED) # Benchmarks
    if (NOT TARGET Catch2::Catch2)
        message(WARNING "Package Catch2::Catch target not found. Building tests will be disabled.")
    endif ()
    if (NOT TARGET benchmark::benchmark)
        message(WARNING "Package benchmark::benchmark target not found. Building benchmarks will be disabled.")
    endif ()
endif ()

if (NOT BUILD_CORE_ONLY)
    include(${PROJECT_MAIN_DIR}/cmake/Hunter/main_packages.cmake)

    # Silence CMake policy warnings in submodules
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW) # project() command manages VERSION variables
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW) # Honor visibility properties for all target types
    set(CMAKE_POLICY_DEFAULT_CMP0074 NEW)

    find_package(Boost CONFIG REQUIRED COMPONENTS context coroutine) # Boost
    find_package(absl CONFIG REQUIRED)
    find_package(CLI11 REQUIRED)

    # MDBX
    set(MDBX_ENABLE_TESTS OFF)
    set(MDBX_ALLOY_BUILD OFF)
    set(MDBX_BUILD_TIMESTAMP "unknown") # For reproducible builds
    add_subdirectory(third-party/libmdbx)
    target_include_directories(mdbx-static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/libmdbx)

    if (NOT TARGET Boost::coroutine OR NOT TARGET Boost::context)
        message(FATAL_ERROR "Mandatory packages Boost::coroutine or Boost::context not found")
    elseif (NOT TARGET OpenSSL::SSL)
        message(FATAL_ERROR "Mandatory package OpenSSL::SSL not found")
    elseif (NOT TARGET absl::time)
        message(FATAL_ERROR "Mandatory package absl::time not found")
    elseif (NOT TARGET mdbx-static)
        message(FATAL_ERROR "Mandatory package mdbx-static not found")
    elseif (NOT TARGET CLI11::CLI11)
        message(FATAL_ERROR "Mandatory package CLI11::CLI11 not found")
    endif ()

endif (NOT BUILD_CORE_ONLY)
message(CHECK_PASS "All required packages found")

# Interface library for third parties
add_library(third-party-includes INTERFACE)
target_include_directories(third-party-includes INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/expected/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/indicators/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/magic_enum/include
)


# Compiler enforcements on ${PROJECT_NAME}
if (NOT MSVC)

    add_compile_options(-Werror -Wall -Wextra -pedantic)
    add_compile_options(-Wshadow -Wimplicit-fallthrough -Wsign-conversion)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>)
    add_compile_options(-Wno-missing-field-initializers -Wnon-virtual-dtor)
    add_compile_options(-Wno-unknown-pragmas) # Ignore unknown pragmas (especially for clang-tidy)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-attributes)

        # coroutines support
        if ((CMAKE_CXX_STANDARD GREATER_EQUAL 20))
            add_compile_options(-fcoroutines)
        endif ()
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME MATCHES "Darwin")
        add_compile_definitions(_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
        add_compile_options(-Wthread-safety)
    endif ()

else ()

    add_compile_options(/permissive-)

    #add_compile_options(/we4100)  # Unreferenced formal parameter

    add_compile_options(/we4834)  # Discarded a [[nodiscard]] value
    #add_compile_options(/we5204)  # Missing Virtual Destructor

    add_compile_options(/we4018)  # Signedness
    add_compile_options(/we4388)  # Signedness
    add_compile_options(/we4245)  # Signedness
    add_compile_options(/we4267)  # Signedness
    add_compile_options(/we4389)  # Signedness

    add_compile_options(/we6244)  # Equivalent to -Wshadow
    add_compile_options(/we6246)  # Equivalent to -Wshadow

    add_compile_options(/we26819) # Equivalent to -Wimplicit-fallthrough

endif ()

add_subdirectory(src) # Main sources

if (NOT BUILD_CORE_ONLY)
    add_subdirectory(cmd) # Executables sources
endif (NOT BUILD_CORE_ONLY)
