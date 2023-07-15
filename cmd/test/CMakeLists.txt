#[[
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

find_package(Catch2 CONFIG REQUIRED)

# Zen Core Tests
message(STATUS "Looking for tests ... ${BUILD_MAIN_SRC_DIR}/core/*_test.cpp")
file(GLOB_RECURSE ZEN_CORE_TESTS CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/core/*_test.cpp")
list(LENGTH ZEN_CORE_TESTS ZEN_CORE_SOURCE_ITEMS)
if (NOT ZEN_CORE_SOURCE_ITEMS EQUAL 0)
    add_executable(core_test unit_test.cpp ${ZEN_CORE_TESTS})
    target_include_directories(core_test PRIVATE ${PROJECT_MAIN_DIR}/third-party/magic_enum/include)

    # No exceptions here
    target_compile_definitions(core_test PRIVATE BOOST_NO_EXCEPTIONS)
    if (MSVC)
        target_compile_options(core_test PRIVATE /EHa- /EHsc)
    else ()
        target_compile_options(core_test PRIVATE -fno-exceptions)
    endif ()

    target_link_libraries(core_test zen_core Catch2::Catch2)

endif ()


if (NOT BUILD_CORE_ONLY)

    # Zen Node Tests
    message(STATUS "Looking for tests ... ${BUILD_MAIN_SRC_DIR}/node/*_test.cpp")
    file(GLOB_RECURSE ZEN_NODE_TESTS CONFIGURE_DEPENDS "${BUILD_MAIN_SRC_DIR}/node/*_test.cpp")
    list(LENGTH ZEN_NODE_TESTS ZEN_NODE_SOURCE_ITEMS)
    if (NOT ZEN_NODE_SOURCE_ITEMS EQUAL 0)
        add_executable(node_test unit_test.cpp ${ZEN_NODE_TESTS})
        target_include_directories(node_test PRIVATE ${PROJECT_MAIN_DIR}/third-party/magic_enum/include)
        target_link_libraries(node_test zen_node Catch2::Catch2)
    endif ()

endif ()
