/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "common.hpp"

#include <map>
#include <regex>
#include <string>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/ip/address.hpp>

#include <zen/core/encoding/hex.hpp>

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

    cli.add_option("--etl.buffersize", etl_buffer_size_str, "Buffer size for ETL operations")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("64MiB", {"1GiB"}));

    cli.add_option("--syncloop.batchsize", batch_size_str, "Batch size for stage execution")
        ->capture_default_str()
        ->check(HumanSizeParserValidator("64MiB", {"16GiB"}));

    cli.add_option("--syncloop.throttle", node_settings.sync_loop_throttle_seconds,
                   "Sets the minimum delay between sync loop starts (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(1u, 7200u));

    cli.add_option("--syncloop.loginterval", node_settings.sync_loop_log_interval_seconds,
                   "Sets the interval between sync loop INFO logs (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(10u, 600u));

    cli.add_flag("--fakepow", node_settings.fake_pow, "Disables proof-of-work verification");
    cli.add_flag("--zk.nochecksums", node_settings.no_zcash_checksums,
                 "Disables initial verification of zk proofs files checksums");

    // Asio settings
    const size_t available_hw_concurrency{std::thread::hardware_concurrency()};
    size_t user_asio_concurrency{std::max((available_hw_concurrency / 2), size_t(2))};

    cli.add_option("--asio.concurrency", user_asio_concurrency, "Concurrency level for asio")
        ->capture_default_str()
        ->check(CLI::Range(size_t(2), available_hw_concurrency));

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
    node_settings.data_directory->deploy();  // Ensure subdirs are created

    node_settings.batch_size = *parse_human_bytes(batch_size_str);
    node_settings.etl_buffer_size = *parse_human_bytes(etl_buffer_size_str);

    node_settings.asio_concurrency = user_asio_concurrency;
    node_settings.asio_context = std::make_unique<boost::asio::io_context>(static_cast<int>(user_asio_concurrency));
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

std::string get_node_name_from_build_info(const buildinfo* build_info) {
    std::string node_name{"zen/"};
    node_name.append(build_info->git_branch);
    node_name.append(" v");
    node_name.append(build_info->project_version);
    node_name.append("/");
    node_name.append(build_info->system_name);
    node_name.append("-");
    node_name.append(build_info->system_processor);
    node_name.append("_");
    node_name.append(build_info->build_type);
    node_name.append("/");
    node_name.append(build_info->compiler_id);
    node_name.append("-");
    node_name.append(build_info->compiler_version);
    return node_name;
}

IPEndPointValidator::IPEndPointValidator(bool allow_empty, int default_port) {
    func_ = [allow_empty, default_port](std::string& value) -> std::string {
        if (value.empty() && allow_empty) {
            return {};
        }

        const std::regex pattern(R"(^([\d\.]*|localhost)(:[\d]{1,4})?$)", std::regex_constants::icase);
        std::smatch matches;
        if (!std::regex_match(value, matches, pattern)) {
            return "Value " + value + " is not a valid endpoint";
        }

        std::string address_part{matches[1].str()};
        std::string port_part{matches[2].str()};

        if (address_part.empty()) {
            address_part = "0.0.0.0";
        } else if (boost::iequals(address_part, "localhost")) {
            address_part = "127.0.0.1";
        }

        if (!port_part.empty()) {
            port_part.erase(0, 1);  // Get rid of initial ":"
        } else {
            if (default_port) {
                port_part = std::to_string(default_port);
            } else {
                return "Value " + value + " does not contain a valid port";
            }
        }

        // Validate IP address
        boost::system::error_code err;
        boost::asio::ip::make_address(address_part, err).to_string();
        if (err) {
            return "Value " + std::string(address_part) + " is not a valid ip address";
        }

        // Validate port
        if (int port{std::stoi(port_part)}; port < 1 || port > 65535) {
            return "Value " + port_part + " is not a valid port";
        }

        value = address_part + ":" + port_part;
        return {};
    };
}
}  // namespace zen::cmd
