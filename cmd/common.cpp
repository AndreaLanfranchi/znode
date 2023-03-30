/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "common.hpp"

namespace zen::cmd {

void parse_node_command_line(CLI::App& cli, int argc, char** argv, Settings& settings) {
    auto& node_settings = settings.node_settings;

    // Node settings
    std::filesystem::path data_dir_path;
    std::string chaindata_max_size_str{to_human_bytes(node_settings.chaindata_env_config.max_size, /*binary=*/true)};
    std::string chaindata_growth_size_str{
        to_human_bytes(node_settings.chaindata_env_config.growth_size, /*binary=*/true)};
    std::string chaindata_page_size_str{to_human_bytes(node_settings.chaindata_env_config.page_size, /*binary=*/true)};
    std::string batch_size_str{to_human_bytes(node_settings.batch_size, /*binary=*/true)};
    std::string etl_buffer_size_str{to_human_bytes(node_settings.etl_buffer_size, /*binary=*/true)};

    cli.add_option("--datadir", data_dir_path, "Path to data directory")
        ->default_val(get_os_default_storage_path().string());

    cli.add_flag("--chaindata.exclusive", node_settings.chaindata_env_config.exclusive,
                 "Chaindata database opened in exclusive mode");
    cli.add_flag("--chaindata.readahead", node_settings.chaindata_env_config.read_ahead,
                 "Chaindata database enable readahead");
    cli.add_flag("--chaindata.writemap", node_settings.chaindata_env_config.write_map,
                 "Chaindata database enable writemap");

    cli.add_option("--chaindata.growthsize", chaindata_growth_size_str, "Chaindata database growth size.")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("64MiB"));
    cli.add_option("--chaindata.pagesize", chaindata_page_size_str, "Chaindata database page size. A power of 2")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("256B", {"65KiB"}));
    cli.add_option("--chaindata.maxsize", chaindata_max_size_str, "Chaindata database max size.")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("32MiB", {"128TiB"}));

    cli.add_option("--batchsize", batch_size_str, "Batch size for stage execution")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("64MiB", {"16GiB"}));
    cli.add_option("--etl.buffersize", etl_buffer_size_str, "Buffer size for ETL operations")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("64MiB", {"1GiB"}));

    cli.add_option("--sync.loop.throttle", node_settings.sync_loop_throttle_seconds,
                   "Sets the minimum delay between sync loop starts (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(1u, 7200u));

    cli.add_option("--sync.loop.log.interval", node_settings.sync_loop_log_interval_seconds,
                   "Sets the interval between sync loop logs (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(10u, 600u));

    cli.add_flag("--fakepow", node_settings.fake_pow, "Disables proof-of-work verification");

    // Logging options
    auto& log_settings = settings.log_settings;
    add_logging_options(cli, log_settings);

    // Parse and validate
    cli.parse(argc, argv);

    auto parsed_size_value{parse_human_bytes(chaindata_page_size_str)};
    if ((*parsed_size_value & (*parsed_size_value - 1)) != 0) {
        throw std::invalid_argument("--chaindata.pagesize value is not a power of 2");
    }
    node_settings.chaindata_env_config.page_size = *parsed_size_value;

    const auto mdbx_max_size_hard_limit{node_settings.chaindata_env_config.page_size * db::kMdbxMaxPages};
    parsed_size_value = parse_human_bytes(chaindata_max_size_str);
    if (*parsed_size_value > mdbx_max_size_hard_limit) {
        throw std::invalid_argument("--chaindata.maxsize is invalid or > " +
                                    to_human_bytes(mdbx_max_size_hard_limit, /*binary=*/true));
    }
    node_settings.chaindata_env_config.max_size = *parsed_size_value;

    parsed_size_value = parse_human_bytes(chaindata_growth_size_str);
    if (*parsed_size_value > (mdbx_max_size_hard_limit / /* two increments ?*/ 2u)) {
        throw std::invalid_argument("--chaindata.growthsize max value > " +
                                    to_human_bytes(mdbx_max_size_hard_limit / 2, /*binary=*/true));
    }
    node_settings.chaindata_env_config.growth_size = *parsed_size_value;

    node_settings.data_directory = std::make_unique<DataDirectory>(data_dir_path);
    node_settings.batch_size = *parse_human_bytes(batch_size_str);
    node_settings.etl_buffer_size = *parse_human_bytes(etl_buffer_size_str);
}

void add_logging_options(CLI::App& cli, log::Settings& log_settings) {
    using enum log::Level;
    std::map<std::string, log::Level, std::less<>> level_mapping{
        {"critical", kCritical}, {"error", kError}, {"warning", kWarning},
        {"info", kInfo},         {"debug", kDebug}, {"trace", kTrace},
    };
    auto& log_opts = *cli.add_option_group("Log", "Logging options");
    log_opts.add_option("--log.verbosity", log_settings.log_verbosity, "Sets log verbosity")
        ->capture_default_str()
        ->check(CLI::Range(kCritical, kTrace))
        ->transform(CLI::Transformer(level_mapping, CLI::ignore_case))
        ->default_val(log_settings.log_verbosity);
    log_opts.add_flag("--log.stdout", log_settings.log_std_out, "Outputs to std::out instead of std::err");
    log_opts.add_flag("--log.nocolor", log_settings.log_nocolor, "Disable colors on log lines");
    log_opts.add_flag("--log.utc", log_settings.log_utc, "Prints log timings in UTC");
    log_opts.add_flag("--log.threads", log_settings.log_threads, "Prints thread ids");
    log_opts.add_option("--log.file", log_settings.log_file, "Tee all log lines to given file name");
}

}  // namespace zen::cmd
