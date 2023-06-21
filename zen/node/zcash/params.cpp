/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "params.hpp"

#include <fstream>
#include <iostream>

#include <gsl/gsl_util>

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
#pragma GCC diagnostic ignored "-Wshadow"
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

#include <curl/curl.h>
#include <curl/easy.h>
#include <magic_enum.hpp>

#include <zen/core/common/misc.hpp>
#include <zen/core/crypto/md.hpp>
#include <zen/core/encoding/hex.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/common/terminal.hpp>
#include <zen/node/concurrency/ossignals.hpp>

namespace zen::zcash {

bool validate_param_files(const std::filesystem::path& directory) {
    std::vector<ParamFile> errored_param_files{};

    for (const auto& param_file : kParamFiles) {
        if (Ossignals::signalled()) {
            return false;
        }
        const auto file_path{directory / param_file.name};
        if (!std::filesystem::exists(file_path)) {
            errored_param_files.push_back(param_file);
            continue;
        }

        if (!std::filesystem::is_regular_file(file_path)) {
            log::Critical("Not a regular file", {"file", file_path.string()}) << "I don't trust to remove it";
            return false;
        }

        if (const auto actual_size{std::filesystem::file_size(file_path)}; actual_size != param_file.expected_size) {
            std::vector<std::string> log_args{"file",     file_path.string(),
                                              "expected", std::to_string(param_file.expected_size),
                                              "actual",   std::to_string(actual_size)};
            if (!std::filesystem::remove(file_path)) {
                log::Critical("Invalid file size", log_args) << "Failed to remove invalid file";
                return false;
            }
            log::Warning("Invalid file size", log_args) << "Removed invalid file";
            errored_param_files.push_back(param_file);
            continue;
        }

        const auto expected_checksum{hex::decode(param_file.expected_checksum)};
        if (!validate_file_checksum(file_path, *expected_checksum)) {
            if (!std::filesystem::remove(file_path)) {
                log::Critical("Invalid file checksum",
                              {"file", file_path.string(), "expected", std::string(param_file.expected_checksum)})
                    << "Failed to remove invalid file";
                return false;
            }
            log::Warning("Invalid file checksum", {"file", file_path.string()}) << "Removed invalid file";
            errored_param_files.push_back(param_file);
            continue;
        }
    }

    if (errored_param_files.empty()) return true;  // All ok

    std::cout << "\n============================================================================================\n"
              << "One or more required param files are missing - or have wrong checksum - in directory\n"
              << directory.string() << ". Files are : \n";

    size_t total_download_size{0};
    for (const auto& param_file : errored_param_files) {
        std::cout << " - " << param_file.name << " [" << to_human_bytes(param_file.expected_size, true) << "]\n";
        total_download_size += param_file.expected_size;
    }

    std::cout << "\nYou can either allow me to download them now, or, if you have them already \n"
              << "under another data directory, you can copy those or link them there. \n"
              << "In any case I cannot proceed without this mandatory files. \n\n"
              << "If you decide to download them now please allow some time:\nit's up to "
              << to_human_bytes(total_download_size, true) << " download." << std::endl;

    if (!ask_user_confirmation("Do you want me to download them now?")) {
        return false;
    }

    for (const auto& param_file : errored_param_files) {
        if (Ossignals::signalled()) {
            return false;
        }
        const auto file_path{directory / param_file.name};
        if (!download_param_file(directory, param_file)) {
            log::Critical("Failed to download param file", {"file", file_path.string()});
            return false;
        }

        // Once again, check file's size and checksum
        if (const auto actual_size{std::filesystem::file_size(file_path)}; actual_size != param_file.expected_size) {
            std::vector<std::string> log_args{"file",     file_path.string(),
                                              "expected", std::to_string(param_file.expected_size),
                                              "actual",   std::to_string(actual_size)};
            log::Critical("Invalid file size (again)", log_args);
            return false;
        }

        const auto expected_checksum{hex::decode(param_file.expected_checksum)};
        if (!validate_file_checksum(file_path, *expected_checksum)) {
            log::Critical("Invalid file checksum (again)",
                          {"file", file_path.string(), "expected", std::string(param_file.expected_checksum)});

            return false;
        }
    }
    return true;
}

std::optional<Bytes> get_file_sha256_checksum(const std::filesystem::path& file_path) {
    using namespace indicators;
    if (!std::filesystem::exists(file_path)) {
        log::Warning("File does not exist", {"file", file_path.string()});
        return std::nullopt;
    }

    const auto total_bytes{std::filesystem::file_size(file_path)};
    std::basic_ifstream<unsigned char, std::char_traits<unsigned char>> file{file_path.string().c_str(),
                                                                             std::ios::in | std::ios::binary};
    if (!file.good()) {
        file.close();
        log::Warning("Failed to open file", {"file", file_path.string()});
        return std::nullopt;
    }

    // Show progress bar
    show_console_cursor(false);
    ProgressBar progress_bar{
        option::BarWidth{50},
        option::Start{"["},
        option::Fill{"="},
        option::Lead{">"},
        option::Remainder{" "},
        option::End{"]"},
        option::PrefixText{"Checksum "},
        option::PostfixText{file_path.filename().string() + " [" + to_human_bytes(total_bytes, true) + "]"},
        option::ForegroundColor{Color::green},
        option::ShowPercentage{true},
        option::ShowElapsedTime{false},
        option::ShowRemainingTime{false},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
        option::MaxProgress{total_bytes}};

    crypto::Sha256 digest;
    Bytes buffer(128_MiB, 0);
    while (file.good()) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        digest.update(ByteView{buffer.data(), static_cast<size_t>(file.gcount())});
        progress_bar.set_progress(progress_bar.current() + static_cast<size_t>(file.gcount()));
    }
    if (!progress_bar.is_completed()) {
        progress_bar.mark_as_completed();
    }
    indicators::show_console_cursor(true);

    file.close();
    return digest.finalize();
}

static int download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                      [[maybe_unused]] curl_off_t ultotal, [[maybe_unused]] curl_off_t ulnow) noexcept {
    using namespace indicators;
    static size_t prev_progress{0};
    if (const size_t progress{dltotal != 0 ? static_cast<size_t>(dlnow * 100 / dltotal) : 0};
        progress != prev_progress) {
        prev_progress = progress;
        auto* progress_bar = static_cast<ProgressBar*>(clientp);
        if (!progress_bar->is_completed()) {
            progress_bar->set_progress(progress);
        }
    }
    return Ossignals::signalled() ? 1 : 0;
}

bool download_param_file(const std::filesystem::path& directory, const ParamFile& param_file) {
    using namespace indicators;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    FILE* file_pointer{nullptr};
    if (!curl) {
        log::Critical("Failed to initialize curl");
        return false;
    }
    auto free_curl{gsl::finally([curl] { curl_easy_cleanup(curl); })};

    std::string url{kTrustedDownloadBaseUrl};
    ZEN_REQUIRE(url.ends_with("/"));
    url.append(param_file.name);

    auto return_code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 1);
    if (return_code == CURLE_OK)
        return_code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);  // Todo : set verififypeer

    // Initialize progress bar
    show_console_cursor(false);
    ProgressBar progress_bar{
        option::BarWidth{50},
        option::Start{"["},
        option::Fill{"="},
        option::Lead{">"},
        option::Remainder{" "},
        option::End{"]"},
        option::PrefixText{"Download "},
        option::PostfixText{std::string(param_file.name) + " [" + to_human_bytes(param_file.expected_size, true) + "]"},
        option::ForegroundColor{Color::green},
        option::ShowPercentage{true},
        option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}};

    auto free_progress{gsl::finally([&progress_bar] {
        if (!progress_bar.is_completed()) {
            progress_bar.mark_as_completed();
        }
        show_console_cursor(true);
    })};

    if (return_code == CURLE_OK)
        return_code = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_bar);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1_KiB * 8);
    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10);

    const auto target_file = (directory / param_file.name);
    if (return_code == CURLE_OK) {
#ifdef _MSC_VER  // Windows
        if (auto error = fopen_s(&file_pointer, target_file.string().c_str(), "wb")) return_code = CURLE_WRITE_ERROR;
#else
        file_pointer = fopen(target_file.string().c_str(), "wb");
#endif
        if (!file_pointer) {
            log::Critical("Failed to open target file", {"file", target_file.string(), "mode", "wb"});
            return false;
        }
    }

    if (return_code == CURLE_OK) return_code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, file_pointer);
    if (return_code == CURLE_OK) return_code = curl_easy_perform(curl);

    if (file_pointer) fclose(file_pointer);

    if (return_code != CURLE_OK) {
        log::Error("Failed to download file",
                   {"file", std::string(param_file.name), "curl_error", std::string(curl_easy_strerror(return_code))});
        std::filesystem::remove(target_file);
        return false;
    }

    // This task is done. The validity of the file will be checked elsewhere
    return true;
}
bool validate_file_checksum(const std::filesystem::path& file_path, ByteView expected_checksum) {
    const auto actual_checksum{get_file_sha256_checksum(file_path)};
    if (!actual_checksum) {
        log::Error("Failed to compute checksum", {"file", file_path.string()});
        return false;
    }
    bool is_match{*actual_checksum == expected_checksum};
    if (!is_match) {
        log::Error("Invalid file checksum", {"file", file_path.string(), "expected", hex::encode(expected_checksum),
                                             "actual", hex::encode(*actual_checksum)});
    }
    return is_match;
}
}  // namespace zen::zcash
