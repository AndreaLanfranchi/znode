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
        Boost::headers
        Boost::thread
        Boost::coroutine # Mind the order
        Boost::context   # of this two
        Boost::timer
        OpenSSL::SSL
        )

set(PRIVATE_LIBS $<$<BOOL:${MSVC}>:ntdll.lib>)
target_link_libraries(${BUILD_INFRA_COMPONENT} PUBLIC ${PUBLIC_LIBS} PRIVATE ${PRIVATE_LIBS})


