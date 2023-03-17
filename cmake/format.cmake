#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} OS_NAME)

if("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "")
    set(ARCH_NAME x64)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL x86_64)
    set(ARCH_NAME x64)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL IA64)
    set(ARCH_NAME x64)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL AMD64)
    set(ARCH_NAME x64)
endif()

find_program(
        CLANG_FORMAT clang-format
        PATHS "third-party/clang-format/${OS_NAME}-${ARCH_NAME}"
        NO_SYSTEM_ENVIRONMENT_PATH
)

cmake_policy(SET CMP0009 NEW)
file(
        GLOB_RECURSE SRC
        LIST_DIRECTORIES false
        "zen/*.?pp"
)

execute_process(COMMAND ${CLANG_FORMAT} -style=file -i ${SRC})