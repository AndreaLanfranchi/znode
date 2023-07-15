#[[
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

# Core Tests
message(CHECK_START "Looking for core tests ...")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "$CACHE{BUILD_MAIN_SRC_DIR}/core/*_test.?pp")
list(LENGTH SOURCES SOURCE_ITEMS)
message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
if (NOT SOURCE_ITEMS EQUAL 0)
    add_executable(core_test unit_test.cpp ${SOURCES})
    target_link_libraries(core_test PRIVATE $CACHE{BUILD_CORE_COMPONENT} Catch2::Catch2)
    target_include_directories(core_test PUBLIC SYSTEM $CACHE{BUILD_3RDP_SRC_DIR}/magic_enum/include)

    # No exceptions here
    target_compile_definitions(core_test PRIVATE BOOST_NO_EXCEPTIONS)
    if (MSVC)
        target_compile_options(core_test PRIVATE /EHa- /EHsc)
    else ()
        target_compile_options(core_test PRIVATE -fno-exceptions)
    endif ()

endif ()

if (NOT BUILD_CORE_ONLY)

    # Main app Tests
    message(CHECK_START "Looking for main tests ...")
    set(SOURCES)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "$CACHE{BUILD_MAIN_SRC_DIR}/main/*_test.?pp")
    list(LENGTH SOURCES SOURCE_ITEMS)
    message(CHECK_PASS "found ${SOURCE_ITEMS} source files")
    if (NOT SOURCE_ITEMS EQUAL 0)
        add_executable(main_test unit_test.cpp ${SOURCES})
        target_include_directories(main_test PUBLIC SYSTEM $CACHE{BUILD_3RDP_SRC_DIR}/magic_enum/include)
        target_link_libraries(main_test PRIVATE $CACHE{BUILD_MAIN_COMPONENT} Catch2::Catch2)
    endif ()

endif ()
