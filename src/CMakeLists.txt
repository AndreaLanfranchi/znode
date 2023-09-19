#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

add_subdirectory(core) # Core components - always built
if (NOT BUILD_CORE_ONLY)
    add_subdirectory(infra) # Infrastructure components
    add_subdirectory(node)  # The node implementation
endif (NOT BUILD_CORE_ONLY)