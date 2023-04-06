/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <stdexcept>

#include <openssl/opensslv.h>

#include <zen/core/common/memory.hpp>

#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/ossignals.hpp>
#include <zen/node/database/access_layer.hpp>
#include <zen/node/database/mdbx_tables.hpp>

#include "common.hpp"

using namespace zen;
using namespace std::chrono;

//! \brief Ensures database is ready and consistent with command line arguments
void prepare_chaindata_env(NodeSettings& node_settings, [[maybe_unused]] bool init_if_not_configured = true) {
    bool chaindata_exclusive{node_settings.chaindata_env_config.exclusive};  // save setting
    {
        auto& config = node_settings.chaindata_env_config;
        config.path = (*node_settings.data_directory)["chaindata"].path().string();
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
            throw std::runtime_error("Unable to detect schema version");
        }
        log::Message("Database schema", {"version", detected_schema_version->to_string()});
        if (*detected_schema_version < zen::db::tables::kRequiredSchemaVersion) {
            // TODO Run migrations and update schema version
            // for the moment an exception is thrown
            std::string what{"Incompatible schema version:"};
            what.append(" expected " + zen::db::tables::kRequiredSchemaVersion.to_string());
            what.append(" got " + detected_schema_version->to_string());
            throw std::runtime_error(what);
        }
    } else {
        db::tables::deploy_tables(*tx, db::tables::kChainDataTables);
        db::write_schema_version(*tx, zen::db::tables::kRequiredSchemaVersion);
        tx.commit(/*renew=*/true);
    }

    tx.commit(/*renew=*/false);
    chaindata_env.close();
    node_settings.chaindata_env_config.exclusive = chaindata_exclusive;  // Restore from CLI
    node_settings.chaindata_env_config.create = false;                   // Already created
}

int main(int argc, char* argv[]) {
    const auto build_info(zen_get_buildinfo());

    CLI::App cli(std::string(build_info->project_name).append(" node"));
    cli.get_formatter()->column_width(50);

    try {
        Ossignals::init();  // Intercept OS signals

        cmd::Settings settings;
        auto& node_settings = settings.node_settings;

        // This parses and validates command line arguments
        // After return we're able to start services. Datadir has been deployed
        cmd::parse_node_command_line(cli, argc, argv, settings);
        cmd::prime_zcash_params((*node_settings.data_directory)["zcash-params"].path());

        // Start logging
        log::init(settings.log_settings);
        log::set_thread_name("main");

        // Output BuildInfo
        settings.node_settings.build_info = cmd::get_node_name_from_build_info(build_info);
        log::Message("Using node", {"version", settings.node_settings.build_info});

        // Output mdbx build info
        auto const& mdbx_ver{mdbx::get_version()};
        auto const& mdbx_bld{mdbx::get_build()};
        log::Message("Using libmdbx",
                     {"version", mdbx_ver.git.describe, "build", mdbx_bld.target, "compiler", mdbx_bld.compiler});
        // Output OpenSSL build info
        log::Message("Using OpenSSL", {"version", OPENSSL_VERSION_TEXT});

        // Check and open db
        prepare_chaindata_env(node_settings, true);
        auto chaindata_env{db::open_env(node_settings.chaindata_env_config)};

        // Start boost asio
        using asio_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
        auto asio_guard = std::make_unique<asio_guard_type>(node_settings.asio_context.get_executor());
        std::thread asio_thread{[&node_settings]() {
            log::set_thread_name("asio");
            log::Message("Service", {"name", "asio", "status", "starting"});
            node_settings.asio_context.run();
            log::Message("Service", {"name", "asio", "status", "stopped"});
        }};

        // Start prometheus endpoint if required
        if (!node_settings.prometheus_endpoint.empty()) {
            log::Message("Service",
                         {"name", "prometheus", "status", "starting", "endpoint", node_settings.prometheus_endpoint});
            node_settings.prometheus_service = std::make_unique<zen::Prometheus>(node_settings.prometheus_endpoint);
        }

        // Start sync loop
        const auto start_time{std::chrono::steady_clock::now()};

        // Keep waiting till sync_loop stops
        // Signals are handled in sync_loop and below
        auto t1{std::chrono::steady_clock::now()};
        const auto chaindata_dir{(*node_settings.data_directory)["chaindata"]};
        const auto etltmp_dir{(*node_settings.data_directory)["etl-tmp"]};

        // TODO while (sync_loop.get_state() != Worker::State::kStopped) {
        while (true) {
            std::this_thread::sleep_for(500ms);

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
                const auto mem_usage{get_mem_usage(true)};
                const auto vmem_usage{get_mem_usage(false)};
                const auto chaindata_usage{chaindata_dir.size(true)};
                const auto etltmp_usage{etltmp_dir.size(true)};

                log::Info("Resource usage", {
                                                "mem",
                                                to_human_bytes(mem_usage, true),
                                                "vmem",
                                                to_human_bytes(vmem_usage, true),
                                                "chain",
                                                to_human_bytes(chaindata_usage, true),
                                                "etl-tmp",
                                                to_human_bytes(etltmp_usage, true),
                                                "uptime",
                                                StopWatch::format(total_duration),
                                            });

                if (node_settings.prometheus_service) {
                    static auto& resources_gauge{node_settings.prometheus_service->set_gauge(
                        "resources_usage", "Node resources usage in bytes")};
                    static auto& resident_memory_gauge{
                        resources_gauge.Add({{"scope", "memory"}, {"type", "resident"}})};
                    static auto& virtual_memory_gauge{resources_gauge.Add({{"scope", "memory"}, {"type", "virtual"}})};
                    static auto& chaindata_gauge{resources_gauge.Add({{"scope", "storage"}, {"type", "chaindata"}})};
                    static auto& etltmp_gauge{resources_gauge.Add({{"scope", "storage"}, {"type", "etl-tmp"}})};

                    resident_memory_gauge.Set(static_cast<double>(mem_usage));
                    virtual_memory_gauge.Set(static_cast<double>(vmem_usage));
                    chaindata_gauge.Set(static_cast<double>(chaindata_usage));
                    etltmp_gauge.Set(static_cast<double>(etltmp_usage));
                }
            }
        }

        asio_guard.reset();
        asio_thread.join();

        if (node_settings.data_directory) {
            log::Message("Closing database", {"path", (*node_settings.data_directory)["chaindata"].path().string()});
            chaindata_env.close();
            // sync_loop.rethrow();  // Eventually throws the exception which caused the stop
        }

        t1 = std::chrono::steady_clock::now();
        const auto total_duration{t1 - start_time};
        log::Info("All done", {"uptime", StopWatch::format(total_duration)});

        return 0;

    } catch (const CLI::ParseError& ex) {
        return cli.exit(ex);
    } catch (const std::runtime_error& ex) {
        log::Error() << ex.what();
        return -1;
    } catch (const std::invalid_argument& ex) {
        std::cerr << "\tInvalid argument :" << ex.what() << "\n" << std::endl;
        return -3;
    } catch (const std::exception& ex) {
        std::cerr << "\tUnexpected error : " << ex.what() << "\n" << std::endl;
        return -4;
    } catch (...) {
        std::cerr << "\tUnexpected undefined error\n" << std::endl;
        return -99;
    }
}