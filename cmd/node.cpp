/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <stdexcept>

#include <zen/buildinfo.h>

#include <zen/core/common/memory.hpp>

#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/ossignals.hpp>

#include "common.hpp"
#include "zen/core/common/misc.hpp"

using namespace zen;
using namespace std::chrono;

int main(int argc, char* argv[]) {
    const auto build_info(zen_get_buildinfo());

    CLI::App cli(std::string(build_info->project_name).append(" node"));
    cli.get_formatter()->column_width(50);

    try {
        Ossignals::init();  // Intercept OS signals

        cmd::CoreSettings settings;
        auto& node_settings = settings.node_settings;

        // TODO parse command line
        (void)argc;
        (void)argv;

        log::init(settings.log_settings);
        log::set_thread_name("main");

        // Output BuildInfo
        settings.node_settings.build_info =
            "version=" + std::string(build_info->git_branch) + std::string(build_info->project_version) +
            "build=" + std::string(build_info->system_name) + "-" + std::string(build_info->system_processor) + " " +
            std::string(build_info->build_type) + "compiler=" + std::string(build_info->compiler_id) + " " +
            std::string(build_info->compiler_version);

        log::Message(
            std::string(build_info->project_name).append(" node"),
            {"version", std::string(build_info->git_branch) + std::string(build_info->project_version), "build",
             std::string(build_info->system_name) + "-" + std::string(build_info->system_processor) + " " +
                 std::string(build_info->build_type),
             "compiler", std::string(build_info->compiler_id) + " " + std::string(build_info->compiler_version)});

        // Output mdbx build info
        auto const& mdbx_ver{mdbx::get_version()};
        auto const& mdbx_bld{mdbx::get_build()};
        log::Message("libmdbx",
                     {"version", mdbx_ver.git.describe, "build", mdbx_bld.target, "compiler", mdbx_bld.compiler});

        // Start boost asio
        using asio_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
        auto asio_guard = std::make_unique<asio_guard_type>(node_settings.asio_context.get_executor());
        std::thread asio_thread{[&node_settings]() -> void {
            log::set_thread_name("asio");
            log::Trace("Boost Asio", {"state", "started"});
            node_settings.asio_context.run();
            log::Trace("Boost Asio", {"state", "stopped"});
        }};

        // Start sync loop
        const auto start_time{std::chrono::steady_clock::now()};
        //        stagedsync::SyncLoop sync_loop(&node_settings, &chaindata_db, block_exchange);
        //        sync_loop.start(/*wait=*/false);

        // Keep waiting till sync_loop stops
        // Signals are handled in sync_loop and below
        auto t1{std::chrono::steady_clock::now()};
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
            if ((t2 - t1) > std::chrono::seconds(10)) {
                std::swap(t1, t2);
                const auto total_duration{t1 - start_time};

                log::Info(
                    "Resource usage",
                    {
                        //
                        "mem", to_human_bytes(get_mem_usage(true), true),  //
                        "vmem",
                        to_human_bytes(
                            get_mem_usage(false),
                            true),  //
                                    //                        "chain",
                                    //                        to_human_bytes((*node_settings.data_directory)["chaindata"].size(true),
                                    //                        true),  //
                                    //                        "etl-tmp",
                                    //                        to_human_bytes((*node_settings.data_directory)["etl-tmp"].size(true),
                                    //                        true),  //
                        "uptime", StopWatch::format(total_duration),  //
                    });
            }
        }

        asio_guard.reset();
        asio_thread.join();

        if (node_settings.data_directory) {
            log::Message() << "Closing database chaindata path: "
                           << (*node_settings.data_directory)["chaindata"].path();
            // chaindata_db.close();
            log::Message() << "Database closed";
            // sync_loop.rethrow();  // Eventually throws the exception which caused the stop
        }

        t1 = std::chrono::steady_clock::now();
        const auto total_duration{t1 - start_time};
        log::Info("All done", {"uptime", StopWatch::format(total_duration)});

        return 0;

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