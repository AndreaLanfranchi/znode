/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include "option.hpp"

#include <infra/concurrency/task.hpp>

namespace zenpp::nat {

Task<net::IPAddress> detect(Option& option);

}  // namespace zenpp::nat