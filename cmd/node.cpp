/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <stdexcept>

#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include <zen/core/common/memory.hpp>

#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/ossignals.hpp>
#include <zen/node/database/access_layer.hpp>
#include <zen/node/database/mdbx_tables.hpp>
#include <zen/node/network/server.hpp>
#include <zen/node/zcash/params.hpp>

#include "common.hpp"

using namespace zen;
using namespace std::chrono;

//! \brief Ensures database is ready and consistent with command line arguments
void prepare_chaindata_env(NodeSettings& node_settings, [[maybe_unused]] bool init_if_not_configured = true) {
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
        if (*detected_schema_version < zen::db::tables::kRequiredSchemaVersion) {
            // TODO Run migrations and update schema version
            // for the moment an exception is thrown
            std::string what{"Incompatible schema version:"};
            what.append(" expected " + zen::db::tables::kRequiredSchemaVersion.to_string());
            what.append(" got " + detected_schema_version->to_string());
            chaindata_env.close(/*dont_sync=*/true);
            throw std::filesystem::filesystem_error(what, std::make_error_code(std::errc::not_supported));
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
        OPENSSL_init();
        Ossignals::init();  // Intercept OS signals

        cmd::Settings settings;
        auto& node_settings = settings.node_settings;

        // This parses and validates command line arguments
        // After return we're able to start services. Datadir has been deployed
        cmd::parse_node_command_line(cli, argc, argv, settings);

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

        // Validate mandatory zcash params
        StopWatch sw(true);
        log::Message("Validating Zcash params",
                     {"directory", (*node_settings.data_directory)[DataDirectory::kZkParamsName].path().string()});
        if (!zcash::validate_param_files((*node_settings.data_directory)[DataDirectory::kZkParamsName].path())) {
            throw std::filesystem::filesystem_error("Invalid Zcash file params",
                                                    std::make_error_code(std::errc::no_such_file_or_directory));
        }
        log::Message("Validated  Zcash params", {"elapsed", StopWatch::format(sw.since_start())});

        // Check and open db
        prepare_chaindata_env(node_settings, true);
        auto chaindata_env{db::open_env(node_settings.chaindata_env_config)};

        // Start boost asio with the number of threads specified by the concurrency hint
        using asio_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
        auto asio_guard = std::make_unique<asio_guard_type>(node_settings.asio_context.get_executor());
        std::vector<std::thread> asio_threads;
        for(int i{0}; i < 4; ++i) {
            asio_threads.emplace_back([&node_settings, i]() {
                std::string thread_name{"asio-" + std::to_string(i)};
                log::set_thread_name(thread_name);
                log::Trace("Service", {"name", thread_name, "status", "starting"});
                node_settings.asio_context.run();
                log::Trace("Service", {"name", thread_name, "status", "stopped"});
            });
        }

        // Start prometheus endpoint if required
        if (!node_settings.prometheus_endpoint.empty()) {
            log::Message("Service",
                         {"name", "prometheus", "status", "starting", "endpoint", node_settings.prometheus_endpoint});
            node_settings.prometheus_service = std::make_unique<zen::Prometheus>(node_settings.prometheus_endpoint);
        }

        // Start networking server
        zen::network::TCPServer net_server(node_settings.asio_context, nullptr, 13383, 30, 10);
        net_server.start();

        // Start sync loop
        const auto start_time{std::chrono::steady_clock::now()};

        // Keep waiting till sync_loop stops
        // Signals are handled in sync_loop and below
        auto t1{std::chrono::steady_clock::now()};
        const auto chaindata_dir{(*node_settings.data_directory)[DataDirectory::kChainDataName]};
        const auto etltmp_dir{(*node_settings.data_directory)[DataDirectory::kEtlTmpName]};

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
                const auto mem_usage{get_memory_usage(true)};
                const auto vmem_usage{get_memory_usage(false)};
                const auto chaindata_usage{chaindata_dir.size(true)};
                const auto etltmp_usage{etltmp_dir.size(true)};

                log::Info("Resource usage", {
                                                "mem",
                                                to_human_bytes(mem_usage, true),
                                                "vmem",
                                                to_human_bytes(vmem_usage, true),
                                                std::string(DataDirectory::kChainDataName),
                                                to_human_bytes(chaindata_usage, true),
                                                std::string(DataDirectory::kEtlTmpName),
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
                    static auto& chaindata_gauge{resources_gauge.Add(
                        {{"scope", "storage"}, {"type", std::string(DataDirectory::kChainDataName)}})};
                    static auto& etltmp_gauge{
                        resources_gauge.Add({{"scope", "storage"}, {"type", std::string(DataDirectory::kEtlTmpName)}})};

                    resident_memory_gauge.Set(static_cast<double>(mem_usage));
                    virtual_memory_gauge.Set(static_cast<double>(vmem_usage));
                    chaindata_gauge.Set(static_cast<double>(chaindata_usage));
                    etltmp_gauge.Set(static_cast<double>(etltmp_usage));
                }
            }
        }

        net_server.stop();  // Stop networking server

        asio_guard.reset(); // Ensure asio is stopped last
        for (auto& asio_thread : asio_threads) asio_thread.join();


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
    } catch (const std::filesystem::filesystem_error& ex) {
        log::Error() << "\tFilesystem error :" << ex.what();
        return -2;
    } catch (const std::invalid_argument& ex) {
        std::cerr << "\tInvalid argument :" << ex.what() << "\n" << std::endl;
        return -3;
    } catch (const db::Exception& ex) {
        std::cerr << "\tUnexpected db error : " << ex.what() << "\n" << std::endl;
        return -4;
    } catch (const std::runtime_error& ex) {
        log::Error() << ex.what();
        return -1;
    } catch (const std::exception& ex) {
        std::cerr << "\tUnexpected error : " << ex.what() << "\n" << std::endl;
        return -5;
    } catch (...) {
        std::cerr << "\tUnexpected undefined error\n" << std::endl;
        return -99;
    }
}