#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

add_subdirectory(core) # Core components - always built

if (NOT BUILD_CORE_ONLY)
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/node)
        message(FATAL_ERROR "No \"app\" directory found, cannot build")
    endif ()
    add_subdirectory(node) # The node implementation
endif (NOT BUILD_CORE_ONLY)