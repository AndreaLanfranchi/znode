/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <memory>
#include <optional>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <core/chain/config.hpp>
#include <core/common/base.hpp>

#include <app/common/directories.hpp>
#include <app/common/log.hpp>
#include <app/database/mdbx.hpp>  // TODO define dbsettings here and remove this include (mdbx.hpp must include this)

namespace zenpp {

struct NetworkSettings {
    std::string local_endpoint{"0.0.0.0:9033"};       // Local node listen address
    bool ipv4_only{false};                            // Whether to listen/connect on IPv4 addresses only
    uint32_t max_active_connections{256};             // Maximum allowed number of connected nodes
    uint32_t max_active_connections_per_ip{1};        // Maximum allowed number of connected nodes per single IP address
    uint32_t protocol_handshake_timeout_seconds{10};  // Number of seconds to wait for protocol handshake completion
    uint32_t inbound_timeout_seconds{10};      // Number of seconds to wait for the completion of an inbound message
    uint32_t outbound_timeout_seconds{10};     // Number of seconds to wait for the completion of an outbound message
    uint32_t idle_timeout_seconds{300};        // Number of seconds after which an inactive node is disconnected
    bool use_tls{true};                        // Whether to enforce SSL/TLS on network connections
    std::string tls_password{};                // Password to use to load a private key file
    std::vector<std::string> connect_nodes{};  // List of nodes to connect to at startup
    uint64_t nonce{0};                         // Local nonce (identifies self in network)
    uint32_t ping_interval_seconds{120};       // Interval between ping messages
    uint32_t ping_timeout_milliseconds{500};   // Number of milliseconds to wait for a ping response before timing-out
};

struct AppSettings {
    size_t asio_concurrency{2};                       // Async context concurrency level
    std::unique_ptr<DataDirectory> data_directory;    // Main data folder
    db::EnvConfig chaindata_env_config{};             // Chaindata db config
    uint32_t network_id{kMainNetConfig.identifier_};  // Network/Chain id
    std::optional<ChainConfig> chain_config;          // Chain config
    size_t batch_size{512_MiB};                       // Batch size to use in stages
    size_t etl_buffer_size{256_MiB};                  // Buffer size for ETL operations
    bool fake_pow{false};                             // Whether to verify Proof-of-Work (PoW)
    bool no_zk_checksums{false};                      // Whether to verify zk files' checksums
    uint32_t sync_loop_throttle_seconds{0};           // Minimum interval amongst sync cycle
    uint32_t sync_loop_log_interval_seconds{30};      // Interval for sync loop to emit logs
    NetworkSettings network{};                        // Network related settings
    log::Settings log{};                              // Log related settings
};
}  // namespace zenpp
