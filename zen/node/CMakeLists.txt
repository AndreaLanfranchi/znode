#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

message(CHECK_START "Looking for required packages ...")
find_package(absl CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE ZEN_NODE_SRC CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.c" "*.h" "*.cc")
list(FILTER ZEN_NODE_SRC EXCLUDE REGEX "_test\\.cpp$")
list(FILTER ZEN_NODE_SRC EXCLUDE REGEX "_benchmark\\.cpp$")
list(LENGTH ZEN_NODE_SRC ZEN_NODE_SRC_ITEMS)

if(NOT ZEN_NODE_SRC_ITEMS EQUAL 0)

    add_library(zen_node ${ZEN_NODE_SRC})

    target_include_directories(zen_node PUBLIC
            ${CMAKE_SOURCE_DIR}
            ${ZEN_MAIN_DIR}/third-party/magic_enum/include)

    set(ZEN_NODE_PUBLIC_LIBS zen_core
            mdbx-static
            nlohmann_json::nlohmann_json
            absl::time
            )

    set(ZEN_NODE_PRIVATE_LIBS CLI11::CLI11)

    if(MSVC)
        list(APPEND ZEN_NODE_PRIVATE_LIBS ntdll.lib)
    endif(MSVC)

    target_link_libraries(zen_node PUBLIC ${ZEN_NODE_PUBLIC_LIBS} PRIVATE ${ZEN_NODE_PRIVATE_LIBS})

endif()

