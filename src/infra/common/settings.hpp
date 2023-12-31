/*
   Copyright 2022 The Silkworm Authors
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

#pragma once
#include <memory>
#include <optional>
#include <vector>

#include <absl/strings/str_cat.h>
#include <boost/asio/io_context.hpp>

#include <core/chain/config.hpp>
#include <core/common/base.hpp>

#include <infra/common/log.hpp>
#include <infra/database/mdbx.hpp>  // TODO define dbsettings here and remove this include (mdbx.hpp must include this)
#include <infra/filesystem/directories.hpp>
#include <infra/nat/option.hpp>

namespace znode {

struct NetworkSettings {
    nat::Option nat{};                                                                   // NAT traversal option
    std::string local_endpoint{absl::StrCat("0.0.0.0:", kMainNetConfig.default_port_)};  // Local node listen address
    bool ipv4_only{false};                            // Whether to listen/connect on IPv4 addresses only
    uint32_t max_active_connections{128};             // Maximum allowed number of connected nodes
    uint32_t max_active_connections_per_ip{1};        // Maximum allowed number of connected nodes per single IP address
    uint32_t min_outgoing_connections{32};            // Minimum number of outgoing connections
    uint32_t protocol_handshake_timeout_seconds{10};  // Number of seconds to wait for protocol handshake completion
    uint32_t inbound_timeout_seconds{10};      // Number of seconds to wait for the completion of an inbound message
    uint32_t outbound_timeout_seconds{10};     // Number of seconds to wait for the completion of an outbound message
    uint32_t idle_timeout_seconds{300};        // Number of seconds after which an inactive node is disconnected
    bool use_tls{true};                        // Whether to enforce SSL/TLS on network connections
    std::string tls_password{};                // Password to use to load a private key file
    std::vector<std::string> connect_nodes{};  // List of nodes to connect to at startup
    bool force_dns_seeding{false};             // Whether to force DNS seeding
    uint32_t connect_timeout_seconds{2};       // Number of seconds to wait for a dial-out socket connection
    uint64_t nonce{0};                         // Local nonce (identifies self in network)
    uint32_t ping_interval_seconds{120};       // Interval between ping messages
    uint32_t ping_timeout_milliseconds{500};   // Number of milliseconds to wait for a ping response before timing-out
};

struct AppSettings {
    size_t asio_concurrency{2};                     // Async context concurrency level
    std::unique_ptr<DataDirectory> data_directory;  // Main data folder
    db::EnvConfig chaindata_env_config{};           // Chaindata db config
    db::EnvConfig nodedata_env_config{
        // Nodedata db config
        .max_size{512_MiB},
        .growth_size{2_MiB},
        .max_tables{32},
        .max_readers{32},
    };
    uint32_t network_id{kMainNetConfig.identifier_};  // Network/Chain id
    std::optional<ChainConfig> chain_config;          // Chain config
    size_t batch_size{512_MiB};                       // Batch size to use in stages
    size_t etl_buffer_size{256_MiB};                  // Buffer size for ETL operations
    bool fake_pow{false};                             // Whether to verify Proof-of-Work (PoW)
    bool no_zk_checksums{false};                      // Whether to verify zk files' checksums
    uint32_t sync_loop_throttle_seconds{0};           // Minimum get_interval amongst sync cycle
    uint32_t sync_loop_log_interval_seconds{30};      // Interval for sync loop to emit logs
    NetworkSettings network{};                        // Network related settings
    log::Settings log{};                              // Log related settings
};
}  // namespace znode
