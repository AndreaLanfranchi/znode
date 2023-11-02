#[[
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

# Core Tests
message(CHECK_START "Looking for core tests ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/core/*_test.?pp")
list(LENGTH SOURCES SOURCE_ITEMS)
message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
if (NOT SOURCE_ITEMS EQUAL 0)
    set(CORE_TESTS_TARGET "${PROJECT_NAME}-core-tests")
    add_executable(${CORE_TESTS_TARGET} unit_test.cpp ${SOURCES})
    target_link_libraries(${CORE_TESTS_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_CORE_COMPONENT} Catch2::Catch2)
    target_include_directories(${CORE_TESTS_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})

    # No exceptions here
    target_compile_definitions(${CORE_TESTS_TARGET} PRIVATE BOOST_NO_EXCEPTIONS)
    if (MSVC)
        target_compile_options(${CORE_TESTS_TARGET} PRIVATE /EHa- /EHsc)
    else ()
        target_compile_options(${CORE_TESTS_TARGET} PRIVATE -fno-exceptions)
    endif ()

endif ()

if (NOT BUILD_CORE_ONLY)

    # Infra Tests
    message(CHECK_START "Looking for infra tests ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/infra/*_test.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        set(INFRA_TESTS_TARGET "${PROJECT_NAME}-infra-tests")
        add_executable(${INFRA_TESTS_TARGET} unit_test.cpp ${SOURCES})
        target_include_directories(${INFRA_TESTS_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})
        target_link_libraries(${INFRA_TESTS_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} Catch2::Catch2)
    endif ()

    # Node Tests
    message(CHECK_START "Looking for node tests ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/node/*_test.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        set(NODE_TESTS_TARGET "${PROJECT_NAME}-node-tests")
        add_executable(${NODE_TESTS_TARGET} unit_test.cpp ${SOURCES})
        target_include_directories(${NODE_TESTS_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})
        target_link_libraries(${NODE_TESTS_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} ${BUILD_INFRA_COMPONENT} Catch2::Catch2)
    endif ()


endif ()
