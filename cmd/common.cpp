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

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4267)
#pragma warning(disable : 4244)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <zen/core/crypto/sha_2_256.hpp>
#include <zen/core/encoding/hex.hpp>

namespace zen::cmd {

using namespace indicators;

int curl_download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                    [[maybe_unused]] curl_off_t ultotal, [[maybe_unused]] curl_off_t ulnow) noexcept {
    static size_t prev_progress{0};
    const size_t progress{dltotal != 0 ? static_cast<size_t>(dlnow * 100 / dltotal) : 0};
    if (progress != prev_progress) {
        prev_progress = progress;
        auto* progress_bar = static_cast<ProgressBar*>(clientp);
        if (!progress_bar->is_completed()) {
            progress_bar->set_progress(progress);
        }
    }
    return 0;
}

void curl_download_file(const std::string& url, const std::filesystem::path& destination_path,
                        const std::optional<std::string> sha256sum) {
    const auto pos{url.find_last_of('/')};
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid file url");
    }
    std::string file_name{url.substr(pos + 1)};
    if (file_name.empty()) {
        throw std::invalid_argument("Invalid file url");
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    FILE* file_pointer{nullptr};

    if (!curl) {
        throw std::runtime_error("Could not initialize curl");
    }

    indicators::show_console_cursor(false);

    // Set progress callback
    ProgressBar progress_bar{option::BarWidth{50},
                             option::Start{"["},
                             option::Fill{"="},
                             option::Lead{">"},
                             option::Remainder{" "},
                             option::End{"]"},
                             option::PrefixText{"Downloading "},
                             option::PostfixText{file_name},
                             option::ForegroundColor{Color::green},
                             option::ShowPercentage{true},
                             option::ShowElapsedTime{true},
                             option::ShowRemainingTime{true},
                             option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}};

    auto return_code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 1);

#ifdef _MSC_VER  // Windows
    // TODO Provide correct cert repository path
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif

    if (return_code == CURLE_OK)
        return_code = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_download_progress_callback);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_bar);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    if (return_code == CURLE_OK) {
        const auto target_file = (destination_path / file_name).string();
#ifdef _MSC_VER  // Windows
        auto error = fopen_s(&file_pointer, target_file.c_str(), "wb");
        if (error) return_code = CURLE_WRITE_ERROR;
#else
        file_pointer = fopen(target_file.c_str(), "wb");
#endif

        if (!file_pointer) return_code = CURLE_WRITE_ERROR;
    }
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, file_pointer);
    if (return_code == CURLE_OK) return_code = curl_easy_perform(curl);
    if (return_code == CURLE_OK) {
        long http_response_code;
        return_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
        if (return_code == CURLE_OK && http_response_code != 200) return_code = CURLE_HTTP_RETURNED_ERROR;
    }

    if (file_pointer) fclose(file_pointer);
    curl_easy_cleanup(curl);
    indicators::show_console_cursor(true);

    if (return_code != CURLE_OK) {
        std::filesystem::remove(destination_path / file_name);
        throw std::runtime_error(curl_easy_strerror(return_code));
    }

    if (sha256sum.has_value() && !sha256sum->empty()) {
        const auto expected_hash = hex::decode(sha256sum.value());
        if (!expected_hash) {
            throw std::invalid_argument("Invalid sha256sum");
        }

        const auto file_path = destination_path / file_name;
        std::ifstream file_stream{file_path.string(), std::ios::in | std::ios::binary};
        if (!file_stream.good()) {
            std::filesystem::remove(file_path);
            throw std::runtime_error("Could not open file " + file_name);
        }
        Bytes buffer(1_MiB, 0);
        crypto::Sha256 hasher;
        while (file_stream.good()) {
            file_stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            hasher.update(ByteView{buffer.data(), static_cast<std::size_t>(file_stream.gcount())});
        }
        file_stream.close();
        const auto actual_hash = hasher.finalize();
        if (actual_hash != expected_hash.value()) {
            std::filesystem::remove(file_path);
            throw std::runtime_error("Invalid sha256sum for file " + file_name);
        }
    }
}

void prime_zcash_params(const std::filesystem::path& params_dir) {
    // Filename -> SHA256 hash
    static const std::map<std::string, std::string> params_files{
        {"sprout-proving.key", "8bc20a7f013b2b58970cddd2e7ea028975c88ae7ceb9259a5344a16bc2c0eef7"},
        {"sprout-verifying.key", "4bd498dae0aacfd8e98dc306338d017d9c08dd0918ead18172bd0aec2fc5df82"},
        {"sapling-output.params", "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4"},
        {"sapling-spend.params", "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13"},
        {"sprout-groth16.params", "b685d700c60328498fbde589c8c7c484c722b788b265b72af448a5bf0ee55b50"}};

    std::vector<std::string> missing_params_files;
    for (const auto& [file_name, hash_string] : params_files) {
        if (!std::filesystem::exists(params_dir / file_name)) {
            missing_params_files.push_back(file_name);
        }
    }
    if (missing_params_files.empty()) {
        return;
    }

    std::cout << "\n===============================================================================\n"
              << "One or more required zcash param files are missing from this directory:\n"
              << params_dir.string() << "\n"
              << "You can either allow me to download them now, or, if you have them already \n"
              << "under another data directory, you can copy those or link them there. \n"
              << "In any case I cannot proceed without this mandatory files. \n\n"
              << "If you decide to download them now please allow some time: it's up to 1.57Gib download. \n";

    if (!ask_user_confirmation("Do you want me to download them now?")) {
        throw std::runtime_error("Missing zcash params");
    }

    const std::string base_url{"https://downloads.horizen.io/file/TrustedSetup/"};
    for (const auto& file_name : missing_params_files) {
        curl_download_file(base_url + file_name, params_dir, params_files.at(file_name));
    }
}

bool ask_user_confirmation(const std::string& message) {
    static std::regex pattern{"^([yY])?([nN])?$"};
    std::smatch matches;
    std::string answer;
    do {
        std::cout << "\n" << message << " [y/N] ";
        std::cin >> answer;
        std::cin.clear();
        if (std::regex_search(answer, matches, pattern, std::regex_constants::match_default)) {
            break;
        }
        std::cout << "Hmmm... maybe you didn't read carefully. I repeat:" << std::endl;
    } while (true);

    return matches[1].length() > 0;
}

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
                   "Sets the interval between sync loop INFO logs (in seconds)")
        ->capture_default_str()
        ->check(CLI::Range(10u, 600u));

    cli.add_flag("--fakepow", node_settings.fake_pow, "Disables proof-of-work verification");

    cli.add_option("--prometheus.endpoint", node_settings.prometheus_endpoint,
                   "Prometheus endpoint listening address\n"
                   "Not set or empty means do not start metrics collection\n"
                   "Use the form ip-address:port\n"
                   "DO NOT EXPOSE TO THE PUBLIC INTERNET")
        ->capture_default_str()
        ->transform(IPEndPointValidator(/*allow_empty=*/true, /*default_port=*/8080));

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
