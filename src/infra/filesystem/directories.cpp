/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "directories.hpp"

#if defined(_MSC_VER)
// clang-format off
#include <windows.h>  // Needed to set target architecture - Keep the order
#include <libloaderapi.h>
// clang-format on
#endif
#include <array>
#include <fstream>

#include <absl/strings/str_cat.h>

#include <core/common/misc.hpp>

#include <infra/os/environment.hpp>

namespace zenpp {

std::filesystem::path get_os_temporary_path() { return std::filesystem::temp_directory_path(); }

std::filesystem::path get_process_absolute_full_path() {
#if defined(_MSC_VER)
    std::array<wchar_t, FILENAME_MAX> path{0};
    GetModuleFileNameW(nullptr, path.data(), FILENAME_MAX);
    return {path.data()};
#else
    std::array<char, FILENAME_MAX> path{0};
    ssize_t count = readlink("/proc/self/exe", path.data(), FILENAME_MAX);
    return std::filesystem::path(std::string(path.data(), (count > 0U) ? static_cast<size_t>(count) : 0U));
#endif
}

std::filesystem::path get_unique_temporary_path(std::optional<std::filesystem::path> base_path) {
    if (not base_path) base_path.emplace(get_os_temporary_path());
    if (not base_path->is_absolute()) base_path = std::filesystem::absolute(*base_path);
    if (not std::filesystem::exists(*base_path) or not std::filesystem::is_directory(*base_path)) {
        throw std::invalid_argument("Path " + base_path->string() + " does not exist or is not a directory");
    }

    // Build random paths appending random strings of fixed length to base path
    // If 1000 attempts fail we throw
    for (int i{0}; i < 1000; ++i) {
        auto temp_generated_absolute_path{*base_path / get_random_alpha_string(10)};
        if (not std::filesystem::exists(temp_generated_absolute_path)) {
            return temp_generated_absolute_path;
        }
    }

    // We were unable to find a valid unique non-existent path
    throw std::filesystem::filesystem_error("Unable to find a valid unique non-existent name in " + base_path->string(),
                                            *base_path, std::make_error_code(std::errc::file_exists));
}

Directory::Directory(const std::filesystem::path& path) : path_(path) {
    if (path_.empty()) path_ = std::filesystem::current_path();
    if (not path.is_absolute()) path_ = std::filesystem::absolute(path_);
    if (not path_.has_filename()) {
        throw std::invalid_argument("Invalid path " + path_.string());
    }
    if (std::filesystem::exists(path_) and !std::filesystem::is_directory(path_)) {
        throw std::invalid_argument("Invalid path " + path_.string() + " not a directory");
    }
    create();
}

bool Directory::is_pristine() const { return std::filesystem::is_empty(path_); }

const std::filesystem::path& Directory::path() const noexcept { return path_; }

void Directory::clear(bool recurse) {
    for (const auto& entry : std::filesystem::directory_iterator(path_)) {
        if (std::filesystem::is_directory(entry) and !recurse) continue;
        std::filesystem::remove_all(entry.path());
    }
}

size_t Directory::size(bool recurse) const {
    size_t ret{0};
    for (auto it{std::filesystem::recursive_directory_iterator(path_)};
         it != std::filesystem::recursive_directory_iterator{}; ++it) {
        if (std::filesystem::is_directory(it->path())) {
            if (not recurse) it.disable_recursion_pending();
        } else if (std::filesystem::is_regular_file(it->path())) {
            ret += std::filesystem::file_size(it->path());
        }
    }
    return ret;
}
bool Directory::exists() const { return std::filesystem::exists(path_); }

void Directory::create() const {
    if (exists()) return;
    if (not std::filesystem::create_directories(path_)) {
        throw std::filesystem::filesystem_error("Unable to create directory " + path_.string(), path_,
                                                std::make_error_code(std::errc::io_error));
    }
}
Directory Directory::operator[](const std::filesystem::path& path) const {
    if (path.empty() or path.is_absolute() or not path.has_filename()) throw std::invalid_argument("Invalid Path");
    const auto target{path_ / path};
    if (not std::filesystem::exists(target) and not std::filesystem::create_directories(target)) {
        throw std::filesystem::filesystem_error("Unable to create directory " + target.string(), target,
                                                std::make_error_code(std::errc::io_error));
    }
    return Directory(target);
}
bool Directory::is_writable() const noexcept {
    std::filesystem::path test_file_name{get_random_alpha_string(8)};
    while (std::filesystem::exists(path_ / test_file_name)) {
        test_file_name = get_random_alpha_string(8);
    }
    std::filesystem::path const test_file_path{path_ / test_file_name};
    std::ofstream test_file{test_file_path.string()};
    if (not test_file.is_open()) return false;
    test_file << "test";
    test_file.close();
    std::filesystem::remove(test_file_path);
    return true;
}
std::filesystem::path DataDirectory::default_path() {
    // Defaulted to executable's directory
    const std::string base_path_str{
        env::get_default_storage_path().value_or(get_process_absolute_full_path().string())};
    const std::string proj_path_str{absl::StrCat(".", get_buildinfo()->project_name)};

    std::filesystem::path base_dir_path{base_path_str};
#ifdef _WIN32
    base_dir_path /= proj_path_str;
#elif __APPLE__
    base_dir_path /= "Library";
    base_dir_path /= "Application Support";
    base_dir_path /= proj_path_str;
#else
    base_dir_path /= ".local";
    base_dir_path /= "share";
    base_dir_path /= proj_path_str;
#endif

    return base_dir_path;
}
}  // namespace zenpp
