#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

message(CHECK_START "Looking for core source files ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.c" "*.h")
list(FILTER SOURCES EXCLUDE REGEX "\\.*\(_test|_benchmark)\\.(c|cpp|h|hpp)$")
list(LENGTH SOURCES SOURCES_COUNT)
if (SOURCES_COUNT EQUAL 0)
    message(CHECK_FAIL "No source files found")
    message(FATAL_ERROR "No inputs to build")
endif ()
message(CHECK_PASS "found ${SOURCES_COUNT} source files")

add_library(${BUILD_CORE_COMPONENT} ${SOURCES})
target_include_directories(${BUILD_CORE_COMPONENT} PRIVATE ${BUILD_MAIN_SRC_DIR})
set_target_properties(${BUILD_CORE_COMPONENT} PROPERTIES LINKER_LANGUAGE CXX)

# No exceptions here
target_compile_definitions(${BUILD_CORE_COMPONENT} PRIVATE BOOST_NO_EXCEPTIONS)
if (MSVC)
    #target_compile_options(${BUILD_CORE_COMPONENT} PRIVATE /EHa- /EHsc)
else ()
    target_compile_options(${BUILD_CORE_COMPONENT} PRIVATE -fno-exceptions)
endif ()

set(PUBLIC_LIBS
        ${BUILDINFO_LIB}
        third-party-includes
        absl::strings
        absl::time
        intx::intx
        Microsoft.GSL::GSL
        nlohmann_json
        OpenSSL::Crypto)

# See https://github.com/microsoft/vcpkg/issues/2621#issuecomment-359374703
set(PRIVATE_LIBS $<$<BOOL:${MSVC}>:Ws2_32.lib>)

target_link_libraries(${BUILD_CORE_COMPONENT} PUBLIC ${PUBLIC_LIBS} PRIVATE ${PRIVATE_LIBS})
