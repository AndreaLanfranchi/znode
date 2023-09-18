#[[
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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

    # Main app Tests
    message(CHECK_START "Looking for main tests ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/app/*_test.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        set(MAIN_TESTS_TARGET "${PROJECT_NAME}-main-tests")
        add_executable(${MAIN_TESTS_TARGET} unit_test.cpp ${SOURCES})
        target_include_directories(${MAIN_TESTS_TARGET} PRIVATE ${BUILD_MAIN_SRC_DIR})
        target_link_libraries(${MAIN_TESTS_TARGET} PUBLIC third-party-includes PRIVATE ${BUILD_NODE_COMPONENT} Catch2::Catch2)
    endif ()

endif ()
