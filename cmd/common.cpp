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
#include <thread>

#include <absl/time/time.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/ip/address.hpp>

namespace zenpp::cmd {

void parse_node_command_line(CLI::App& cli, int argc, char** argv, AppSettings& settings) {
    auto& network_settings = settings.network;

    // Node settings
    std::filesystem::path data_dir_path;
    std::string chaindata_max_size_str{to_human_bytes(settings.chaindata_env_config.max_size, /*binary=*/true)};
    std::string chaindata_growth_size_str{to_human_bytes(settings.chaindata_env_config.growth_size, /*binary=*/true)};
    std::string chaindata_page_size_str{to_human_bytes(settings.chaindata_env_config.page_size, /*binary=*/true)};
    std::string batch_size_str{to_human_bytes(settings.batch_size, /*binary=*/true)};
    std::string etl_buffer_size_str{to_human_bytes(settings.etl_buffer_size, /*binary=*/true)};

    cli.add_option("--datadir", data_dir_path, "Path to data directory")
        ->default_val(get_os_default_storage_path().string());

    cli.add_option("--chain", settings.network_id, "Name or ID of the network to join (default \"mainnet\")")
        ->capture_default_str()
        ->transform(CLI::Transformer(get_known_chains_map(), CLI::ignore_case));

    cli.add_flag("--chaindata.exclusive", settings.chaindata_env_config.exclusive,
                 "Chaindata database opened in exclusive mode");
    cli.add_flag("--chaindata.readahead", settings.chaindata_env_config.read_ahead,
                 "Chaindata database enable readahead");
    cli.add_flag("--chaindata.writemap", settings.chaindata_env_config.write_map, "Chaindata database enable writemap");

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

    cli.add_option("--syncloop.throttle", settings.sync_loop_throttle_seconds,
                   "Sets the minimum delay between sync loop starts (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(1U, 7200U));

    cli.add_option("--syncloop.loginterval", settings.sync_loop_log_interval_seconds,
                   "Sets the interval between sync loop INFO logs (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(10U, 600U));

    cli.add_flag("--fakepow", settings.fake_pow, "Disables proof-of-work verification");
    cli.add_flag("--zk.nochecksums", settings.no_zk_checksums,
                 "Disables initial verification of zk proofs files checksums");

    // Asio settings
    const size_t available_hw_concurrency{std::thread::hardware_concurrency()};
    size_t user_asio_concurrency{std::max((available_hw_concurrency / 2), size_t(2))};

    cli.add_option("--asio.concurrency", user_asio_concurrency, "Concurrency level for asio")
        ->capture_default_str()
        ->check(CLI::Range(size_t(2), available_hw_concurrency));

    // Network settings
    auto& network_opts = *cli.add_option_group("Network", "Networking options");
    network_opts.add_option("--network.localendpoint", network_settings.local_endpoint, "Local node listening address")
        ->capture_default_str()
        ->check(IPEndPointValidator(/*allow_empty=*/true,
                                    /*default_port=*/9033));  // TODO the port will be on behalf of network

    auto* notls_flag = network_opts.add_flag("--network.notls", "Disable TLS secure communications");

    network_opts.add_option("--network.pkpwd", network_settings.tls_password, "Private key password")
        ->capture_default_str()
        ->excludes(notls_flag);

    network_opts.add_flag("--network.ipv4only", network_settings.ipv4_only, "Listen/connect on IPv4 addresses only");

    network_opts
        .add_option("--network.maxactiveconnections", network_settings.max_active_connections,
                    "Maximum number of concurrent connected nodes")
        ->capture_default_str()
        ->check(CLI::Range(size_t(32), size_t(256)));

    network_opts
        .add_option("--network.maxconnectionsperip", network_settings.max_active_connections_per_ip,
                    "Maximum number of connections allowed from a single IP address")
        ->capture_default_str()
        ->check(CLI::Range(size_t(1), size_t(16)));

    network_opts
        .add_option(
            "--network.handshaketimeout", network_settings.protocol_handshake_timeout_seconds,
            "Number of seconds to wait for a protocol handshake to complete once a TCP connection is established")
        ->capture_default_str()
        ->check(CLI::Range(uint32_t(5), uint32_t(30)));

    network_opts
        .add_option("--network.inboundtimeout", network_settings.inbound_timeout_seconds,
                    "Max number of seconds an inbound message can take to be fully received")
        ->capture_default_str()
        ->check(CLI::Range(uint32_t(5), uint32_t(30)));

    network_opts
        .add_option("--network.idletimeout", network_settings.idle_timeout_seconds,
                    "Number of seconds after which an idle node gets disconnected")
        ->capture_default_str()
        ->check(CLI::Range(size_t(30), size_t(3600)));

    network_opts
        .add_option("--network.pinginterval", network_settings.ping_interval_seconds,
                    "Interval (in seconds) amongst outgoing pings (eventually randomized in a +/- 30% range)")
        ->capture_default_str()
        ->check(CLI::Range(size_t(30), size_t(3600)));

    network_opts
        .add_option("--network.pingtimeout", network_settings.ping_timeout_milliseconds,
                    "Interval (in milliseconds) before a ping without response is considered timed-out")
        ->capture_default_str()
        ->check(CLI::Range(size_t(100), size_t(5000)));

    network_opts
        .add_option("--network.connect", network_settings.connect_nodes,
                    "Immediately connect to this remote nodes list (space separated)")
        ->capture_default_str()
        ->check(IPEndPointValidator(/*allow_empty=*/true,
                                    /*default_port=*/13383));  // TODO the port will be on behalf of network

    // Logging options
    auto& log_settings = settings.log;
    add_logging_options(cli, log_settings);

    // Parse and validate
    cli.parse(argc, argv);

    auto parsed_size_value{parse_human_bytes(chaindata_page_size_str)};
    if ((*parsed_size_value & (*parsed_size_value - 1)) != 0) {
        throw std::invalid_argument("--chaindata.pagesize value is not a power of 2");
    }
    settings.chaindata_env_config.page_size = *parsed_size_value;

    const auto mdbx_max_size_hard_limit{settings.chaindata_env_config.page_size * db::kMdbxMaxPages};
    parsed_size_value = parse_human_bytes(chaindata_max_size_str);
    if (*parsed_size_value > mdbx_max_size_hard_limit) {
        throw std::invalid_argument("--chaindata.maxsize is invalid or > " +
                                    to_human_bytes(mdbx_max_size_hard_limit, /*binary=*/
                                                   true));
    }
    settings.chaindata_env_config.max_size = *parsed_size_value;

    parsed_size_value = parse_human_bytes(chaindata_growth_size_str);
    if (*parsed_size_value > (mdbx_max_size_hard_limit / /* two increments ?*/ 2U)) {
        throw std::invalid_argument("--chaindata.growthsize max value > " +
                                    to_human_bytes(mdbx_max_size_hard_limit / 2, /*binary=*/true));
    }

    settings.chaindata_env_config.growth_size = *parsed_size_value;
    settings.data_directory = std::make_unique<DataDirectory>(data_dir_path);
    settings.data_directory->deploy();  // Ensure subdirs are created
    settings.batch_size = *parse_human_bytes(batch_size_str);
    settings.etl_buffer_size = *parse_human_bytes(etl_buffer_size_str);
    settings.asio_concurrency = user_asio_concurrency;
    network_settings.use_tls = !*notls_flag;
}

void add_logging_options(CLI::App& cli, log::Settings& log_settings) {
    using enum log::Level;
    const std::map<std::string, log::Level, std::less<>> level_mapping{
        {"critical", kCritical}, {"error", kError}, {"warning", kWarning},
        {"info", kInfo},         {"debug", kDebug}, {"trace", kTrace},
    };
    auto& log_opts = *cli.add_option_group("Log", "Logging options");
    log_opts.add_option("--log.verbosity", log_settings.log_verbosity, "Sets log verbosity")
        ->capture_default_str()
        ->check(CLI::Range(kCritical, kTrace))
        ->transform(CLI::Transformer(level_mapping, CLI::ignore_case))
        ->default_val(log_settings.log_verbosity);

    /* TODO implement timezones
        log_opts.add_option("--log.timezone", log_settings.log_timezone, "Sets log timezone. If not specified UTC is
       used")
            ->capture_default_str()
            ->check(TimeZoneValidator(*/
    /*allow_empty=*//*
    false))
            ->default_val(log_settings.log_timezone);
    */

    log_opts.add_flag("--log.stdout", log_settings.log_std_out, "Outputs to std::out instead of std::err");
    log_opts.add_flag("--log.nocolor", log_settings.log_nocolor, "Disable colors on log lines");
    log_opts.add_flag("--log.threads", log_settings.log_threads, "Prints thread ids");
    log_opts.add_option("--log.file", log_settings.log_file, "Tee all log lines to given file name");
}

TimeZoneValidator::TimeZoneValidator(bool allow_empty) {
    description("a valid IANA timezone");
    func_ = [allow_empty](std::string& value) -> std::string {
        if (value.empty() and allow_empty) return {};
        if (value.empty()) value = "a value MUST be specified";
        return {};
    };
}

IPEndPointValidator::IPEndPointValidator(bool allow_empty, uint16_t default_port) {
    func_ = [allow_empty, default_port](std::string& value) -> std::string {
        if (value.empty() && allow_empty) {
            return {};
        }

        boost::asio::ip::address address;
        uint16_t port{0};

        if (!try_parse_ip_address_and_port(value, address, port)) {
            return "Value \"" + value + "\" is not a valid endpoint";
        }
        if (port == 0U) {
            port = static_cast<uint16_t>(default_port);
        }

        value = address.to_string() + ":" + std::to_string(port);
        return {};
    };
}

}  // namespace zenpp::cmd
