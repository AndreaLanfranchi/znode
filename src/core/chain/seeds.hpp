/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <core/chain/config.hpp>

namespace zenpp {

const std::vector<std::string>& get_chain_seeds(const ChainConfig& chain_config);

}  // namespace zenpp