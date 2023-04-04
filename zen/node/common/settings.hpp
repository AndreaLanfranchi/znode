/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <memory>
#include <optional>

#include <boost/asio/io_context.hpp>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <zen/core/common/base.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/database/mdbx.hpp>

namespace zen {

struct NodeSettings {
    std::string build_info{};                                   // Human-readable build info
    boost::asio::io_context asio_context;                       // Async context (e.g. for timers)
    std::unique_ptr<DataDirectory> data_directory;              // Pointer to data folder
    db::EnvConfig chaindata_env_config{};                       // Chaindata db config
    std::string prometheus_endpoint{};                          // Prometheus endpoint
    std::unique_ptr<prometheus::Exposer> prometheus_exposer;    // Prometheus server
    std::unique_ptr<prometheus::Registry> prometheus_registry;  // Prometheus Registry
    size_t batch_size{512_MiB};                                 // Batch size to use in stages
    size_t etl_buffer_size{256_MiB};                            // Buffer size for ETL operations
    bool fake_pow{false};                                       // Whether to verify Proof-of-Work (PoW)
    uint32_t sync_loop_throttle_seconds{0};                     // Minimum interval amongst sync cycle
    uint32_t sync_loop_log_interval_seconds{30};                // Interval for sync loop to emit logs
};

}  // namespace zen
