#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

hunter_add_package(abseil)
hunter_add_package(Boost COMPONENTS context coroutine chrono timer thread)
hunter_add_package(CLI11)
