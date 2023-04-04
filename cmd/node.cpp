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

#include "common.hpp"

using namespace zen;
using namespace std::chrono;

int main(int argc, char* argv[]) {
    const auto build_info(zen_get_buildinfo());

    CLI::App cli(std::string(build_info->project_name).append(" node"));
    cli.get_formatter()->column_width(50);

    try {
        Ossignals::init();  // Intercept OS signals

        cmd::Settings settings;
        auto& node_settings = settings.node_settings;

        // This parses and validates command line arguments
        // After return we're able to start services
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
            node_settings.prometheus_exposer = std::make_unique<prometheus::Exposer>(node_settings.prometheus_endpoint);
            log::Message("Service",
                         {"name", "prometheus", "status", "started", "endpoint", node_settings.prometheus_endpoint});
            node_settings.prometheus_registry = std::make_unique<prometheus::Registry>();
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
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Check signals
            if (Ossignals::signalled()) {
                // sync_loop.stop(true);
                // continue;
                break;
            }

            auto t2{std::chrono::steady_clock::now()};
            if ((t2 - t1) > std::chrono::seconds(60)) {
                std::swap(t1, t2);
                const auto total_duration{t1 - start_time};

                log::Info("Resource usage", {
                                                "mem",
                                                to_human_bytes(get_mem_usage(true), true),
                                                "vmem",
                                                to_human_bytes(get_mem_usage(false), true),
                                                "chain",
                                                to_human_bytes(chaindata_dir.size(true), true),
                                                "etl-tmp",
                                                to_human_bytes(etltmp_dir.size(true), true),
                                                "uptime",
                                                StopWatch::format(total_duration),
                                            });
            }
        }

        asio_guard.reset();
        asio_thread.join();

        if (node_settings.data_directory) {
            log::Message("Closing database", {"path", (*node_settings.data_directory)["chaindata"].path().string()});
            // chaindata_db.close();
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