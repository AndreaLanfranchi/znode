#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

message(CHECK_START "Looking for required packages ...")
find_package(Boost CONFIG REQUIRED COMPONENTS context coroutine)
find_package(OpenSSL REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if (NOT TARGET Boost::coroutine OR NOT TARGET Boost::context)
    message(FATAL_ERROR "Boost components not found")
elseif (NOT TARGET OpenSSL::SSL)
    message(FATAL_ERROR "OpenSSL::SSL not found")
elseif (NOT TARGET absl::time)
    message(FATAL_ERROR "absl::time not found")
elseif (NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "nlohmann_json::nlohmann_json not found")
elseif (NOT TARGET CLI11::CLI11)
    message(FATAL_ERROR "CLI11::CLI11 not found")
elseif (NOT TARGET mdbx-static)
    message(FATAL_ERROR "mdbx-static not found")
endif ()

message(CHECK_PASS "All required packages found")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.c" "*.h")
list(FILTER SOURCES EXCLUDE REGEX "\\.*\(_test|_benchmark)\\.(c|cpp|h|hpp)$")

list(LENGTH SOURCES SOURCES_COUNT)
if (SOURCES_COUNT EQUAL 0)
    message(FATAL_ERROR "No source files found")
endif ()

message(STATUS "Found ${SOURCES_COUNT} source files")
add_library($CACHE{BUILD_MAIN_COMPONENT} ${SOURCES})
target_include_directories($CACHE{BUILD_MAIN_COMPONENT} PUBLIC
        $CACHE{BUILD_3RDP_SRC_DIR}/magic_enum/include
        $CACHE{BUILD_3RDP_SRC_DIR}/indicators/include)

set(PUBLIC_LIBS
        $CACHE{BUILD_CORE_COMPONENT}
        absl::time
        mdbx-static
        nlohmann_json::nlohmann_json
        Boost::coroutine # Mind the order
        Boost::context   # of this two
        OpenSSL::SSL
        )

set(PRIVATE_LIBS $<$<BOOL:${MSVC}>:ntdll.lib>)
target_link_libraries($CACHE{BUILD_MAIN_COMPONENT} PUBLIC ${PUBLIC_LIBS} PRIVATE ${PRIVATE_LIBS})


