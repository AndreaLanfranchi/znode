/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "common.hpp"

#include <iostream>
#include <stdexcept>

#include <boost/timer/timer.hpp>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include <core/common/memory.hpp>

#include <infra/common/stopwatch.hpp>
#include <infra/concurrency/context.hpp>
#include <infra/os/signals.hpp>

#include <node/database/access_layer.hpp>
#include <node/database/mdbx_tables.hpp>
#include <node/network/node_hub.hpp>
#include <node/zk/params.hpp>

using namespace zenpp;
using namespace std::chrono;

//! \brief Ensures database is ready and consistent with command line arguments
void prepare_chaindata_env(AppSettings& node_settings, [[maybe_unused]] bool init_if_not_configured = true) {
    node_settings.data_directory->deploy();
    const bool chaindata_exclusive{node_settings.chaindata_env_config.exclusive};  // save setting
    {
        auto& config = node_settings.chaindata_env_config;
        config.path = (*node_settings.data_directory)[DataDirectory::kChainDataName].path().string();
        config.create = !std::filesystem::exists(db::get_datafile_path(config.path));
        config.exclusive = true;  // Need to open exclusively to apply migrations (if any)
    }

    // Open chaindata environment and check tables
    std::ignore = log::Message("Opening database", {"path", node_settings.chaindata_env_config.path});
    auto chaindata_env{db::open_env(node_settings.chaindata_env_config)};
    db::RWTxn txn(chaindata_env);

    if (not chaindata_env.is_pristine()) {
        // We have already initialized with schema
        const auto detected_schema_version{db::read_schema_version(*txn)};
        if (not detected_schema_version.has_value()) {
            throw db::Exception("Unable to detect schema version");
        }
        std::ignore = log::Message("Database schema", {"version", detected_schema_version->to_string()});
        if (*detected_schema_version < db::tables::kRequiredSchemaVersion) {
            // TODO Run migrations and update schema version
            // for the moment an exception is thrown
            const std::string what{absl::StrCat("Incomplete schema version: expected ",
                                                db::tables::kRequiredSchemaVersion.to_string(), " got ",
                                                detected_schema_version->to_string())};
            chaindata_env.close(/*dont_sync=*/true);
            throw std::filesystem::filesystem_error(what, std::make_error_code(std::errc::not_supported));
        }
    } else {
        db::tables::deploy_tables(*txn, db::tables::kChainDataTables);
        db::write_schema_version(*txn, db::tables::kRequiredSchemaVersion);
        txn.commit(/*renew=*/true);
    }

    // Check db is initialized with chain config
    node_settings.chain_config = db::read_chain_config(*txn);
    if (not node_settings.chain_config.has_value() && init_if_not_configured) {
        const auto chain_config{lookup_known_chain(node_settings.network_id)};
        if (not chain_config.has_value()) throw std::logic_error("Unknown chain");
        db::write_chain_config(*txn, *chain_config->second);
        txn.commit(/*renew=*/true);
        node_settings.chain_config = db::read_chain_config(*txn);
    }
    if (not node_settings.chain_config.has_value()) throw std::logic_error("Unable to read chain config");
    if (node_settings.chain_config->identifier_ != node_settings.network_id) {
        const std::string what{absl::StrCat("Incompatible chain config: requested '",
                                            lookup_known_chain_name(node_settings.network_id), "' have '",
                                            lookup_known_chain_name(node_settings.chain_config->identifier_),
                                            "'. You might want to specify a different data directory.")};
        throw std::logic_error(what);
    }
    std::ignore = log::Message("Chain", {"config", to_string(node_settings.chain_config->to_json())});

    txn.commit(/*renew=*/false);
    chaindata_env.close();
    node_settings.chaindata_env_config.exclusive = chaindata_exclusive;  // Restore from CLI
    node_settings.chaindata_env_config.create = false;                   // Already created
}

int main(int argc, char* argv[]) {

    const auto start_time{std::chrono::steady_clock::now()};
    const auto* build_info(get_buildinfo());
    CLI::App cli(std::string(build_info->project_name).append(" node"));
    cli.get_formatter()->column_width(50);

    try {
        // Initialize OpenSSL
        OPENSSL_init();
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        OpenSSL_add_all_ciphers();
        ERR_load_crypto_strings();

        os::Signals::init();     // Intercept OS signals
        AppSettings settings{};  // Global app settings
        const auto& network_settings = settings.network;
        const auto& log_settings = settings.log;

        // This parses and validates command line arguments
        // After return we're able to start services. Datadir has been deployed
        cmd::parse_node_command_line(cli, argc, argv, settings);

        // Start logging
        log::init(log_settings);
        log::set_thread_name("main");

        // Output BuildInfo
        std::ignore =
            log::Message("Using " + std::string(get_buildinfo()->project_name), {"version", get_buildinfo_string()});

        // Output mdbx build info
        const auto& mdbx_ver{mdbx::get_version()};
        const auto& mdbx_bld{mdbx::get_build()};
        std::ignore = log::Message("Using libmdbx", {"version", mdbx_ver.git.describe, "build", mdbx_bld.target,
                                                     "compiler", mdbx_bld.compiler});
        // Output OpenSSL build info
        std::ignore = log::Message("Using OpenSSL", {"version", OPENSSL_VERSION_TEXT});

        // Check and open db
        prepare_chaindata_env(settings, true);
        auto chaindata_env{db::open_env(settings.chaindata_env_config)};

        // Start boost asio with the number of threads specified by the concurrency hint
        con::Context context("main", settings.asio_concurrency);  // Initialize asio context
        context.start();

        // Check required certificate and key file are present to initialize SSL context
        if (network_settings.use_tls) {
            auto const ssl_data{(*settings.data_directory)[DataDirectory::kSSLCertName].path()};
            if (not net::validate_tls_requirements(ssl_data, network_settings.tls_password)) {
                throw std::filesystem::filesystem_error("Invalid SSL certificate or key file",
                                                        std::make_error_code(std::errc::no_such_file_or_directory));
            }
        }

        // Validate mandatory zk params
        boost::timer::cpu_timer zk_timer;
        const auto zk_params_path{(*settings.data_directory)[DataDirectory::kZkParamsName].path()};
        std::ignore = log::Message("Validating ZK params", {"directory", zk_params_path.string()});
        if (not zk::validate_param_files(*context, zk_params_path, settings.no_zk_checksums)) {
            throw std::filesystem::filesystem_error("Invalid ZK file params",
                                                    std::make_error_code(std::errc::no_such_file_or_directory));
        }
        zk_timer.stop();
        std::ignore = log::Message("Validated  ZK params", {"elapsed", zk_timer.format()});

        // 1) Instantiate and start a new NodeHub
        net::NodeHub node_hub{settings, *context};
        node_hub.start();

        // Keep waiting till sync_loop stops
        // Signals are handled in sync_loop and below
        auto loop_time1{start_time};
        const auto chaindata_dir{(*settings.data_directory)[DataDirectory::kChainDataName]};
        const auto etltmp_dir{(*settings.data_directory)[DataDirectory::kEtlTmpName]};
        const auto nodes_dir{(*settings.data_directory)[DataDirectory::kNodesName]};

        // Count how much time the node hub has been without any connection
        // If it's too long, we'll stop the node
        StopWatch node_hub_idle_sw(true);

        // TODO while (sync_loop.get_state() != Worker::ComponentStatus::kStopped) {
        while (true) {
            std::this_thread::sleep_for(500ms);

            if (node_hub.size() not_eq 0) {
                node_hub_idle_sw.start(true);  // Restart the timer
            } else if (node_hub_idle_sw.since_start() > 5min /* TODO settings.node_hub_idle_timeout*/) {
                log::Warning("Service", {"name", "node_hub", "status", "idle", "elapsed", "5min"})
                    << "Shutting down ...";
                break;
            }

            // Check signals
            if (os::Signals::signalled()) {
                // sync_loop.stop(true);
                // continue;
                break;
            }

            auto loop_time2{std::chrono::steady_clock::now()};
            if ((loop_time2 - loop_time1) > 30s) {
                std::swap(loop_time1, loop_time2);

                const auto total_duration{loop_time1 - start_time};
                const auto mem_usage{get_memory_usage(true)};
                const auto vmem_usage{get_memory_usage(false)};
                const auto chaindata_usage{chaindata_dir.size(true)};
                const auto etltmp_usage{etltmp_dir.size(true)};
                const auto nodes_usage{nodes_dir.size(true)};

                log::Info("Resource usage",
                          {"mem", to_human_bytes(mem_usage, true), "vmem", to_human_bytes(vmem_usage, true),
                           std::string(DataDirectory::kChainDataName), to_human_bytes(chaindata_usage, true),
                           std::string(DataDirectory::kEtlTmpName), to_human_bytes(etltmp_usage, true),
                           std::string(DataDirectory::kNodesName), to_human_bytes(nodes_usage, true), "uptime",
                           StopWatch::format(total_duration)});
            }
        }

        node_hub.stop();  // 1) Stop networking server

        std::ignore = log::Message("Closing database", {"path", chaindata_dir.path().string()});
        chaindata_env.close();
        // sync_loop.rethrow();  // Eventually throws the exception which caused the stop

    } catch (const CLI::ParseError& ex) {
        return cli.exit(ex);
    } catch (const boost::system::error_code& ec) {
        LOG_ERROR << "Boost error code :" << ec.message();
        return -1;
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR << "Filesystem error :" << ex.what();
        return -2;
    } catch (const std::invalid_argument& ex) {
        LOG_ERROR << "Invalid argument :" << ex.what();
        return -3;
    } catch (const db::Exception& ex) {
        LOG_ERROR << "Unexpected db error : " << ex.what();
        return -4;
    } catch (const std::runtime_error& ex) {
        LOG_ERROR << "Unexpected runtime error : " << ex.what();
        return -1;
    } catch (const std::exception& ex) {
        LOG_ERROR << "Unexpected error : " << ex.what();
        return -5;
    } catch (...) {
        LOG_ERROR << "Unexpected undefined error";
        return -99;
    }

    const auto total_duration{std::chrono::steady_clock::now() - start_time};
    std::ignore = log::Info("All done", {"uptime", StopWatch::format(total_duration)});
    return 0;
}