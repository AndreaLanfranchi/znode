/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include "option.hpp"

#include <infra/concurrency/task.hpp>

namespace znode::nat {

Task<void> resolve(Option& option);

}  // namespace znode::nat