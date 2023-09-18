/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "seeds.hpp"

#include <core/common/assert.hpp>

namespace zenpp {

namespace {

    const std::vector<std::string> kMainNetSeeds{
        // "dnsseed.horizen.global",  // Dns Seeder - Unreliable
        // "dnsseed.zensystem.io",    // Dns Seeder - Unreliable
        "mainnet.horizen.global",  // Fixed IP
        "mainnet.zensystem.io",    // Fixed IP
        "node1.zenchain.info"      // Fixed IP
    };

    const std::vector<std::string> kTestNetSeeds{
        // "dnsseed.testnet.horizen.global",  // Dns Seeder - Unreliable
        // "dnsseed.testnet.zensystem.io",    // Dns Seeder - Unreliable
        "testnet.horizen.global",  // Fixed IP
        "testnet.zensystem.io",    // Fixed IP
        "node1.zenchain.info"      // Fixed IP
    };

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