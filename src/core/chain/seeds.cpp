/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "seeds.hpp"

#include <core/common/assert.hpp>

namespace znode {

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
}  // namespace znode
