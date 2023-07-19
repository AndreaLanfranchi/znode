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

#include <core/common/base.hpp>

#include <app/common/directories.hpp>
#include <app/common/log.hpp>
#include <app/database/mdbx.hpp>
#include <app/network/node_hub.hpp>

namespace zenpp {

struct NetworkSettings {
    std::string local_endpoint{"0.0.0.0:13383"};  // Local node listen address
    uint32_t max_active_connections{100};         // Maximum allowed number of connected nodes
    uint32_t idle_timeout_seconds{300};           // Number of seconds after which an inactive node is disconnected
    bool use_tls{true};                           // Whether to enforce SSL/TLS on network connections
    std::string tls_password{};                   // Password to use to load a private key file
};

struct AppSettings {
    size_t asio_concurrency{2};                     // Async context concurrency level
    std::unique_ptr<DataDirectory> data_directory;  // Main data folder
    db::EnvConfig chaindata_env_config{};           // Chaindata db config
    size_t batch_size{512_MiB};                     // Batch size to use in stages
    size_t etl_buffer_size{256_MiB};                // Buffer size for ETL operations
    bool fake_pow{false};                           // Whether to verify Proof-of-Work (PoW)
    bool no_zk_checksums{false};                    // Whether to verify zk files' checksums
    uint32_t sync_loop_throttle_seconds{0};         // Minimum interval amongst sync cycle
    uint32_t sync_loop_log_interval_seconds{30};    // Interval for sync loop to emit logs
    NetworkSettings network{};                      // Network related settings
    log::Settings log{};                            // Log related settings
};

struct AppContext {
    AppSettings settings{};                                 // Node settings
    std::unique_ptr<boost::asio::io_context> asio_context;  // Async context
    std::unique_ptr<network::NodeHub> node_hub;             // Node hub instance
};
}  // namespace zenpp
