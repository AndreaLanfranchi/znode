/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/chain/seeds.hpp>
#include <core/common/assert.hpp>

namespace zenpp {

namespace {

    const std::vector<std::string> kMainNetSeeds{"dnsseed.horizen.global", "dnsseed.zensystem.io",
                                                 "mainnet.horizen.global", "mainnet.zensystem.io",
                                                 "node1.zenchain.info"};

    const std::vector<std::string> kTestNetSeeds{"dnsseed.testnet.horizen.global", "dnsseed.testnet.zensystem.io",
                                                 "testnet.horizen.global", "testnet.zensystem.io",
                                                 "node1.zenchain.info"};

    const std::vector<std::string> kRegTestSeeds{/* there are no seeders for regtest */};

}  // namespace

const std::vector<std::string>& get_chain_seeds(const ChainConfig& chain_config) {

    switch (chain_config.identifier_) {
        case 1:
            return kMainNetSeeds;
        case 2:
            return kTestNetSeeds;
        case 3:
            return kRegTestSeeds;
        default:
            ASSERT(false);  // Should not happen
    }
}
}  // namespace zenpp