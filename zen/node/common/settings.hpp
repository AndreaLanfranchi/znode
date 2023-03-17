/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <memory>
#include <optional>

#include <zen/core/common/base.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/database/mdbx.hpp>

namespace zen {

struct NodeSettings {
    std::string build_info{};                       // Human-readable build info
    std::unique_ptr<DataDirectory> data_directory;  // Pointer to data folder
    db::EnvConfig chaindata_env_config{};           // Chaindata db config
    size_t batch_size{512_MiB};                     // Batch size to use in stages
    size_t etl_buffer_size{256_MiB};                // Buffer size for ETL operations
    bool fake_pow{false};                           // Whether to verify Proof-of-Work (PoW)
    uint32_t sync_loop_throttle_seconds{0};         // Minimum interval amongst sync cycle
    uint32_t sync_loop_log_interval_seconds{30};    // Interval for sync loop to emit logs
};

}  // namespace zen
