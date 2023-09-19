#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

message(CHECK_START "Looking for infra source files ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.c" "*.h")
list(FILTER SOURCES EXCLUDE REGEX "\\.*\(_test|_benchmark)\\.(c|cpp|h|hpp)$")
list(LENGTH SOURCES SOURCES_COUNT)
if (SOURCES_COUNT EQUAL 0)
    message(CHECK_FAIL "No source files found")
    message(FATAL_ERROR "No inputs to build")
endif ()
message(CHECK_PASS "found ${SOURCES_COUNT} source files")

add_library(${BUILD_INFRA_COMPONENT} ${SOURCES})
target_include_directories(${BUILD_INFRA_COMPONENT} PRIVATE ${BUILD_MAIN_SRC_DIR})
set_target_properties(${BUILD_INFRA_COMPONENT} PROPERTIES LINKER_LANGUAGE CXX)

set(PUBLIC_LIBS
        third-party-includes
        ${BUILD_CORE_COMPONENT}
        absl::time
        Boost::coroutine # Mind the order
        Boost::context   # of this two
        OpenSSL::SSL
        )

set(PRIVATE_LIBS $<$<BOOL:${MSVC}>:ntdll.lib>)
target_link_libraries(${BUILD_INFRA_COMPONENT} PUBLIC ${PUBLIC_LIBS} PRIVATE ${PRIVATE_LIBS})


