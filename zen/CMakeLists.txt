#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

add_subdirectory(core) # Core components

if(NOT ZEN_CORE_ONLY)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/node)
        add_subdirectory(node) # The node instance
    endif()
endif(NOT ZEN_CORE_ONLY)