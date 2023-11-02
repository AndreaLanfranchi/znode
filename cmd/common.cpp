/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "common.hpp"

#include "common/nat_validator.hpp"
#include "common/size_validator.hpp"

#include <map>
#include <regex>
#include <string>
#include <thread>

namespace znode::cmd {

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
        ->default_val(DataDirectory::default_path().string());

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
        ->check(common::SizeValidator("64MiB"));
    cli.add_option("--chaindata.pagesize", chaindata_page_size_str, "Chaindata database page size. A power of 2")
        ->capture_default_str()
        ->check(common::SizeValidator("256B", {"65KiB"}));
    cli.add_option("--chaindata.maxsize", chaindata_max_size_str, "Chaindata database max size.")
        ->capture_default_str()
        ->check(common::SizeValidator("32MiB", {"128TiB"}));

    cli.add_option("--etl.buffersize", etl_buffer_size_str, "Buffer size for ETL operations")
        ->capture_default_str()
        ->check(common::SizeValidator("64MiB", {"1GiB"}));

    cli.add_option("--syncloop.batchsize", batch_size_str, "Batch size for stage execution")
        ->capture_default_str()
        ->check(common::SizeValidator("64MiB", {"16GiB"}));

    cli.add_option("--syncloop.throttle", settings.sync_loop_throttle_seconds,
                   "Sets the minimum delay between sync loop starts (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(1U, 7200U));

    cli.add_option("--syncloop.loginterval", settings.sync_loop_log_interval_seconds,
                   "Sets the get_interval between sync loop INFO logs (in seconds)")
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
                                    /*default_port=*/0));

    network_opts.add_option("--network.nat", network_settings.nat, "")
        ->capture_default_str()
        ->check(common::NatOptionValidator());

    auto* notls_flag = network_opts.add_flag("--network.notls", "Disable TLS secure communications");

    network_opts.add_option("--network.pkpwd", network_settings.tls_password, "Private key password")
        ->capture_default_str()
        ->excludes(notls_flag);

    network_opts.add_flag("--network.ipv4only", network_settings.ipv4_only, "Listen/connect on IPv4 addresses only");

    network_opts
        .add_option("--network.maxactiveconnections", network_settings.max_active_connections,
                    "Maximum number of concurrent connected nodes")
        ->capture_default_str()
        ->check(CLI::Range(size_t(16), size_t(128)));

    network_opts
        .add_option("--network.minoutgoingconnections", network_settings.min_outgoing_connections,
                    "Minimum number of outgoing connections to remote nodes")
        ->capture_default_str()
        ->check(CLI::Range(size_t(0), size_t(128)));

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
                                    /*default_port=*/kMainNetConfig.default_port_));

    network_opts
        .add_option("--network.connecttimeout", network_settings.connect_timeout_seconds,
                    "Number of seconds to wait for a dial-out socket connection to complete")
        ->capture_default_str()
        ->check(CLI::Range(uint32_t(1), uint32_t(5)));

    network_opts.add_flag("--network.forcednsseed", network_settings.force_dns_seeding,
                          "Force DNS seeding even if connect nodes are specified or loaded from nodes data");

    // Logging options
    auto& log_settings = settings.log;
    add_logging_options(cli, log_settings);

    // Parse and validate
    cli.parse(argc, argv);

    auto parsed_size{parse_human_bytes(chaindata_page_size_str)};
    settings.chaindata_env_config.page_size = parsed_size.value();
    if ((settings.chaindata_env_config.page_size & (settings.chaindata_env_config.page_size - 1)) != 0) {
        throw std::invalid_argument("--chaindata.pagesize value is not a power of 2");
    }

    const auto mdbx_max_size_hard_limit{settings.chaindata_env_config.page_size * db::kMdbxMaxPages};
    parsed_size = parse_human_bytes(chaindata_max_size_str);
    if (parsed_size.value() > mdbx_max_size_hard_limit) {
        throw std::invalid_argument("--chaindata.maxsize is invalid or > " +
                                    to_human_bytes(mdbx_max_size_hard_limit, /*binary=*/
                                                   true));
    }
    settings.chaindata_env_config.max_size = parsed_size.value();

    parsed_size = parse_human_bytes(chaindata_growth_size_str);
    if (parsed_size.value() > (mdbx_max_size_hard_limit / /* two increments ?*/ 2U)) {
        throw std::invalid_argument("--chaindata.growthsize max value > " +
                                    to_human_bytes(mdbx_max_size_hard_limit / 2, /*binary=*/true));
    }


    // Check number of allowed connections is consistent
    if (settings.network.min_outgoing_connections > settings.network.max_active_connections) {
        throw std::invalid_argument(
            "--network.minoutgoingconnections cannot be greater than "
            "--network.maxactiveconnections");
    };

    settings.chaindata_env_config.growth_size = parsed_size.value();
    settings.data_directory = std::make_unique<DataDirectory>(data_dir_path);
    settings.data_directory->deploy();  // Ensure subdirs are created
    settings.batch_size = parse_human_bytes(batch_size_str).value();
    settings.etl_buffer_size = parse_human_bytes(etl_buffer_size_str).value();
    settings.asio_concurrency = user_asio_concurrency;
    network_settings.use_tls = !*notls_flag;
}

void add_logging_options(CLI::App& cli, log::Settings& log_settings) {
    using enum log::Level;

    const auto level_label = [](log::Level level) {
        std::string ret{magic_enum::enum_name(level)};
        ret.erase(0, 1);
        std::ranges::transform(ret, ret.begin(), [](unsigned char c) { return std::tolower(c); });
        return ret;
    };

    std::map<std::string, log::Level, std::less<>> level_mapping;
    for (const auto enumerator : magic_enum::enum_values<log::Level>()) {
        auto label{level_label(enumerator)};
        level_mapping.try_emplace(label, enumerator);
    }

    auto& log_opts = *cli.add_option_group("Log", "Logging options");
    log_opts.add_option("--log.verbosity", log_settings.log_verbosity, "Sets log verbosity")
        ->capture_default_str()
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

IPEndPointValidator::IPEndPointValidator(bool allow_empty, uint16_t default_port) {
    description("a valid IP endpoint");
    func_ = [allow_empty, default_port](std::string& value) -> std::string {
        if (value.empty() && allow_empty) {
            return {};
        }

        auto parsed_value{net::IPEndpoint::from_string(value)};
        if (parsed_value.has_error()) {
            return "Value \"" + value + "\" is not a valid endpoint";
        }
        parsed_value.value().port_ = parsed_value.value().port_ == 0 ? default_port : parsed_value.value().port_;
        value = parsed_value.value().to_string();
        return {};
    };
}

}  // namespace znode::cmd
