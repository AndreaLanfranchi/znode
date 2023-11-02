/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
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

#include <magic_enum.hpp>
#include <openssl/ssl.h>

#include <core/common/misc.hpp>
#include <core/crypto/md.hpp>
#include <core/encoding/hex.hpp>

#include <infra/common/log.hpp>
#include <infra/os/signals.hpp>

namespace znode::zk {

bool validate_param_files(boost::asio::io_context& asio_context, const std::filesystem::path& directory,
                          bool no_checksums) {
    std::vector<ParamFile> errored_param_files{};

    for (const auto& param_file : kParamFiles) {
        if (os::Signals::signalled()) {
            return false;
        }
        const auto file_path{directory / param_file.name};
        if (not std::filesystem::exists(file_path)) {
            errored_param_files.push_back(param_file);
            continue;
        }

        if (not std::filesystem::is_regular_file(file_path)) {
            log::Critical("Not a regular file", {"file", file_path.string()}) << "I don't trust to remove it";
            return false;
        }

        if (const auto actual_size{std::filesystem::file_size(file_path)}; actual_size != param_file.expected_size) {
            const std::vector<std::string> log_args{"file",     file_path.string(),
                                                    "expected", std::to_string(param_file.expected_size),
                                                    "actual",   std::to_string(actual_size)};
            if (not std::filesystem::remove(file_path)) {
                log::Critical("Invalid file size", log_args) << "Failed to remove invalid file";
                return false;
            }
            log::Warning("Invalid file size", log_args) << "Removed invalid file";
            errored_param_files.push_back(param_file);
            continue;
        }

        if (no_checksums) continue;  // Only first cycle. If we have to download the checksums are already checked
        const auto expected_checksum{enc::hex::decode(param_file.expected_checksum)};
        ASSERT_POST(expected_checksum and "Invalid checksum");
        if (not validate_file_checksum(file_path, expected_checksum.value())) {
            if (not std::filesystem::remove(file_path)) {
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

    if (not ask_user_confirmation("Do you want me to download them now?")) {
        return false;
    }

    for (const auto& param_file : errored_param_files) {
        if (os::Signals::signalled()) {
            return false;
        }
        const auto file_path{directory / param_file.name};
        if (not download_param_file(asio_context, directory, param_file)) {
            log::Critical("Failed to download param file", {"file", file_path.string()});
            return false;
        }

        // Once again, check file's size and checksum
        if (const auto actual_size{std::filesystem::file_size(file_path)}; actual_size != param_file.expected_size) {
            const std::vector<std::string> log_args{"file",     file_path.string(),
                                                    "expected", std::to_string(param_file.expected_size),
                                                    "actual",   std::to_string(actual_size)};
            log::Critical("Invalid file size (again)", log_args);
            return false;
        }

        const auto expected_checksum{enc::hex::decode(param_file.expected_checksum)};
        ASSERT_POST(expected_checksum and "Invalid checksum");
        if (not validate_file_checksum(file_path, expected_checksum.value())) {
            log::Critical("Invalid file checksum (again)",
                          {"file", file_path.string(), "expected", std::string(param_file.expected_checksum)});

            return false;
        }
    }
    return true;
}

std::optional<Bytes> get_file_sha256_checksum(const std::filesystem::path& file_path) {
    using namespace indicators;
    if (not std::filesystem::exists(file_path)) {
        log::Warning("File does not exist", {"file", file_path.string()});
        return std::nullopt;
    }

    const auto total_bytes{std::filesystem::file_size(file_path)};
    // Note ! On linux opening a basic_fstream<unsigned char> in binary mode
    // causes the stream to never read anything. Hence, we open it as char and
    // cast the buffer accordingly. No issues instead with Windows and MSVC
    std::ifstream file{file_path.string().c_str(), std::ios_base::in | std::ios_base::binary};
    if (not file.is_open()) {
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
    Bytes buffer(32_MiB, 0);
    auto* buffer_data{reinterpret_cast<char*>(buffer.data())};
    file.seekg(0, std::ios::beg);
    while (not file.eof()) {
        // Note ! Ignore the result of file.read() as it may be 0
        // also when the number of bytes read is LT the number of max bytes
        // requested
        file.read(buffer_data, static_cast<std::streamsize>(buffer.size()));
        auto bytes_read{static_cast<size_t>(file.gcount())};
        if (bytes_read not_eq 0U) {
            digest.update(ByteView{buffer.data(), bytes_read});
            progress_bar.set_progress(progress_bar.current() + bytes_read);
        }
    }
    if (not progress_bar.is_completed()) {
        progress_bar.mark_as_completed();
    }
    indicators::show_console_cursor(true);

    file.close();
    return digest.finalize();
}

bool download_param_file(boost::asio::io_context& asio_context, const std::filesystem::path& directory,
                         const ParamFile& param_file) {
    using namespace indicators;
    namespace ssl = boost::asio::ssl;
    namespace ip = boost::asio::ip;

    ssl::context ssl_context(ssl::context::tls_client);

    SSL_CTX_set_mode(ssl_context.native_handle(), SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_min_proto_version(ssl_context.native_handle(), TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ssl_context.native_handle(), TLS1_3_VERSION);
    SSL_CTX_set_options(ssl_context.native_handle(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                                                         SSL_OP_NO_RENEGOTIATION |
                                                         SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

    SSL_CTX_set_cipher_list(ssl_context.native_handle(), "HIGH:!aNULL:!eNULL:!NULL:kRSA:!PSK:!SRP:!MD5:!RC4:");
    ssl_context.set_verify_mode(ssl::verify_none);
    ssl_context.set_default_verify_paths();

    ssl::stream<ip::tcp::socket> ssl_stream{asio_context, ssl_context};

    SSL_ctrl(static_cast<SSL*>(ssl_stream.native_handle()), SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name,
             const_cast<void*>(static_cast<const void*>(kTrustedDownloadHost.data())));

    ip::tcp::resolver resolver{asio_context};
    const ip::tcp::resolver::query query(kTrustedDownloadHost.data(), "https");
    auto endpoints = resolver.resolve(query);

    boost::system::error_code error_code;
    boost::asio::connect(ssl_stream.next_layer(), endpoints, error_code);
    if (error_code) {
        log::Error("Failed to connect to server",
                   {"host", std::string(kTrustedDownloadHost), "error", error_code.message()});
        return false;
    }

    ssl_stream.set_verify_mode(ssl::verify_none);  // TODO ! Verify certificate
    ssl_stream.handshake(ssl::stream_base::client, error_code);
    if (error_code) {
        log::Error("Failed to perform SSL handshake",
                   {"host", std::string(kTrustedDownloadHost), "error", error_code.message()});
        return false;
    }

    const auto target_file = (directory / param_file.name);
    std::ofstream file{target_file, std::ios_base::out | std::ios_base::binary};
    if (not file.is_open()) {
        log::Error("Failed to open file", {"file", target_file.string()});
        return false;
    }
    auto close_file{gsl::finally([&file] { file.close(); })};

    // Send request ...
    std::string request = "GET " + std::string(kTrustedDownloadPath) + std::string(param_file.name) +
                          " HTTP/1.1\r\nHost: " + std::string(kTrustedDownloadHost) + "\r\n" + "User-Agent: zen++\r\n" +
                          "Accept: */*\r\n" + "Connection: close\r\n\r\n";

    boost::asio::write(ssl_stream, boost::asio::buffer(request), error_code);
    if (error_code) {
        log::Error("Failed to async_send request",
                   {"host", std::string(kTrustedDownloadHost), "error", error_code.message()});
        return false;
    }

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
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
        option::MaxProgress{param_file.expected_size}};

    auto free_progress{gsl::finally([&progress_bar] {
        if (not progress_bar.is_completed()) {
            progress_bar.mark_as_completed();
        }
        show_console_cursor(true);
    })};

    // Read response ...
    constexpr size_t buffer_size{256_KiB};
    std::array<char, buffer_size> data{0};
    auto buffer = boost::asio::buffer(data);
    bool headers_completed{false};
    size_t bytes_read{static_cast<size_t>(boost::asio::read(ssl_stream, buffer, error_code))};
    while (bytes_read not_eq 0U) {
        if (not headers_completed) [[unlikely]] {
            std::string response(data.data(), bytes_read);
            auto pos = response.find("\r\n\r\n");
            if (pos != std::string::npos) {
                headers_completed = true;
                auto bytes_to_write{bytes_read - pos - 4};
                if (bytes_to_write > 0) {
                    file.write(response.data() + pos + 4, static_cast<std::streamsize>(bytes_to_write));
                    progress_bar.set_progress(progress_bar.current() + bytes_to_write);
                }
            }
        } else {
            file.write(data.data(), static_cast<std::streamsize>(bytes_read));
            progress_bar.set_progress(progress_bar.current() + bytes_read);
        }
        bytes_read = static_cast<size_t>(boost::asio::read(ssl_stream, buffer, error_code));
    }
    if (error_code and error_code not_eq boost::asio::error::eof) {
        log::Error("Failed to read response",
                   {"host", std::string(kTrustedDownloadHost), "error", error_code.message()});
        return false;
    }

    // This task is done. The validity of the file will be checked elsewhere
    return true;
}
bool validate_file_checksum(const std::filesystem::path& file_path, ByteView expected_checksum) {
    const auto actual_checksum{get_file_sha256_checksum(file_path)};
    if (not actual_checksum) {
        log::Error("Failed to compute checksum", {"file", file_path.string()});
        return false;
    }
    const bool is_match{*actual_checksum == expected_checksum};
    if (not is_match) {
        log::Error("Invalid file checksum",
                   {"file", file_path.string(), "expected", enc::hex::encode(expected_checksum), "actual",
                    enc::hex::encode(*actual_checksum)});
    }
    return is_match;
}
}  // namespace znode::zk
