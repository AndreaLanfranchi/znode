/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <stdexcept>

#include <gsl/gsl_util>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include <core/common/memory.hpp>

#include <app/common/stopwatch.hpp>
#include <app/concurrency/ossignals.hpp>
#include <app/database/access_layer.hpp>
#include <app/database/mdbx_tables.hpp>
#include <app/network/node_hub.hpp>
#include <app/zk/params.hpp>

#include "common.hpp"

using namespace zenpp;
using namespace std::chrono;

//! \brief Ensures database is ready and consistent with command line arguments
void prepare_chaindata_env(AppSettings& node_settings, [[maybe_unused]] bool init_if_not_configured = true) {
    bool chaindata_exclusive{node_settings.chaindata_env_config.exclusive};  // save setting
    {
        auto& config = node_settings.chaindata_env_config;
        config.path = (*node_settings.data_directory)[DataDirectory::kChainDataName].path().string();
        config.create = !std::filesystem::exists(db::get_datafile_path(config.path));
        config.exclusive = true;  // Need to open exclusively to apply migrations (if any)
    }

    // Open chaindata environment and check tables
    log::Message("Opening database", {"path", node_settings.chaindata_env_config.path});
    auto chaindata_env{db::open_env(node_settings.chaindata_env_config)};
    db::RWTxn tx(chaindata_env);
    if (!chaindata_env.is_pristine()) {
        // We have already initialized with schema
        const auto detected_schema_version{db::read_schema_version(*tx)};
        if (!detected_schema_version.has_value()) {
            throw db::Exception("Unable to detect schema version");
        }
        log::Message("Database schema", {"version", detected_schema_version->to_string()});
        if (*detected_schema_version < db::tables::kRequiredSchemaVersion) {
            // TODO Run migrations and update schema version
            // for the moment an exception is thrown
            std::string what{"Incompatible schema version:"};
            what.append(" expected " + db::tables::kRequiredSchemaVersion.to_string());
            what.append(" got " + detected_schema_version->to_string());
            chaindata_env.close(/*dont_sync=*/true);
            throw std::filesystem::filesystem_error(what, std::make_error_code(std::errc::not_supported));
        }
    } else {
        db::tables::deploy_tables(*tx, db::tables::kChainDataTables);
        db::write_schema_version(*tx, db::tables::kRequiredSchemaVersion);
        tx.commit(/*renew=*/true);
    }

    tx.commit(/*renew=*/false);
    chaindata_env.close();
    node_settings.chaindata_env_config.exclusive = chaindata_exclusive;  // Restore from CLI
    node_settings.chaindata_env_config.create = false;                   // Already created
}

int main(int argc, char* argv[]) {
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

        Ossignals::init();       // Intercept OS signals
        AppSettings settings{};  // Global app settings
        auto& network_settings = settings.network;
        auto& log_settings = settings.log;

        // This parses and validates command line arguments
        // After return we're able to start services. Datadir has been deployed
        cmd::parse_node_command_line(cli, argc, argv, settings);

        // Start logging
        log::init(log_settings);
        log::set_thread_name("main");

        // Output BuildInfo
        log::Message("Using " + std::string(get_buildinfo()->project_name), {"version", get_buildinfo_string()});

        // Output mdbx build info
        auto const& mdbx_ver{mdbx::get_version()};
        auto const& mdbx_bld{mdbx::get_build()};
        log::Message("Using libmdbx",
                     {"version", mdbx_ver.git.describe, "build", mdbx_bld.target, "compiler", mdbx_bld.compiler});
        // Output OpenSSL build info
        log::Message("Using OpenSSL", {"version", OPENSSL_VERSION_TEXT});

        // Check and open db
        prepare_chaindata_env(settings, true);
        auto chaindata_env{db::open_env(settings.chaindata_env_config)};

        // Start boost asio with the number of threads specified by the concurrency hint
        boost::asio::io_context asio_context(static_cast<int>(settings.asio_concurrency));
        using asio_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
        auto asio_guard = std::make_unique<asio_guard_type>(asio_context.get_executor());
        std::vector<std::thread> asio_threads;
        for (size_t i{0}; i < settings.asio_concurrency; ++i) {
            asio_threads.emplace_back([&asio_context, i]() {
                const std::string thread_name{"asio-" + std::to_string(i)};
                log::set_thread_name(thread_name);
                log::Trace("Service", {"name", thread_name, "status", "starting"});
                asio_context.run();
                log::Trace("Service", {"name", thread_name, "status", "stopped"});
            });
        }
        [[maybe_unused]] const auto stop_asio{gsl::finally([&asio_context, &asio_guard, &asio_threads]() {
            asio_context.stop();
            asio_guard.reset();
            for (auto& t : asio_threads) {
                if (t.joinable()) t.join();
            }
        })};

        // Let some time to allow threads to properly start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check required certificate and key file are present to initialize SSL context
        if (network_settings.use_tls) {
            auto const ssl_data{(*settings.data_directory)[DataDirectory::kSSLCert].path()};
            if (!network::validate_tls_requirements(ssl_data, network_settings.tls_password)) {
                throw std::filesystem::filesystem_error("Invalid SSL certificate or key file",
                                                        std::make_error_code(std::errc::no_such_file_or_directory));
            }
        }

        // Validate mandatory zk params
        StopWatch sw(true);
        auto zk_params_path{(*settings.data_directory)[DataDirectory::kZkParamsName].path()};
        log::Message("Validating ZK params", {"directory", zk_params_path.string()});
        if (!zk::validate_param_files(asio_context, zk_params_path, settings.no_zk_checksums)) {
            throw std::filesystem::filesystem_error("Invalid ZK file params",
                                                    std::make_error_code(std::errc::no_such_file_or_directory));
        }
        log::Message("Validated  ZK params", {"elapsed", StopWatch::format(sw.since_start())});

        // 1) Instantiate and start a new NodeHub
        network::NodeHub node_hub{settings, asio_context};
        node_hub.start();

        // Start sync loop
        const auto start_time{std::chrono::steady_clock::now()};

        // Keep waiting till sync_loop stops
        // Signals are handled in sync_loop and below
        auto t1{start_time};
        const auto chaindata_dir{(*settings.data_directory)[DataDirectory::kChainDataName]};
        const auto etltmp_dir{(*settings.data_directory)[DataDirectory::kEtlTmpName]};
        const auto nodes_dir{(*settings.data_directory)[DataDirectory::kNodesName]};

        // Count how much time the node hub has been without any connection
        // If it's too long, we'll stop the node
        StopWatch node_hub_idle_sw(true);

        // TODO while (sync_loop.get_state() != Worker::State::kStopped) {
        while (true) {
            std::this_thread::sleep_for(500ms);
            if (node_hub.active_connections_count() == 0) {
                if (node_hub_idle_sw.since_start() > 5min /* TODO settings.node_hub_idle_timeout*/) {
                    log::Warning("Service", {"name", "node_hub", "status", "idle", "elapsed", "5min"})
                        << "Shutting down ...";
                    break;
                }
            } else {
                node_hub_idle_sw.start(true);  // Restart the timer
            }

            // Check signals
            if (Ossignals::signalled()) {
                // sync_loop.stop(true);
                // continue;
                break;
            }

            auto t2{std::chrono::steady_clock::now()};
            if ((t2 - t1) > std::chrono::seconds(30)) {
                std::swap(t1, t2);

                const auto total_duration{t1 - start_time};
                const auto mem_usage{get_memory_usage(true)};
                const auto vmem_usage{get_memory_usage(false)};
                const auto chaindata_usage{chaindata_dir.size(true)};
                const auto etltmp_usage{etltmp_dir.size(true)};
                const auto nodes_usage{nodes_dir.size(true)};

                log::Info("Resource usage", {
                                                "mem",
                                                to_human_bytes(mem_usage, true),
                                                "vmem",
                                                to_human_bytes(vmem_usage, true),
                                                std::string(DataDirectory::kChainDataName),
                                                to_human_bytes(chaindata_usage, true),
                                                std::string(DataDirectory::kEtlTmpName),
                                                to_human_bytes(etltmp_usage, true),
                                                std::string(DataDirectory::kNodesName),
                                                to_human_bytes(nodes_usage, true),
                                                "uptime",
                                                StopWatch::format(total_duration),
                                            });
            }
        }

        node_hub.stop(true);  // 1) Stop networking server

        if (settings.data_directory) {
            log::Message("Closing database", {"path", (*settings.data_directory)["chaindata"].path().string()});
            chaindata_env.close();
            // sync_loop.rethrow();  // Eventually throws the exception which caused the stop
        }

        t1 = std::chrono::steady_clock::now();
        const auto total_duration{t1 - start_time};
        log::Info("All done", {"uptime", StopWatch::format(total_duration)});

        return 0;

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
}