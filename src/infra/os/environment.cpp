/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "environment.hpp"

#include <boost/process/environment.hpp>

namespace znode::env {

std::optional<std::string> get(const std::string& name) noexcept {
    auto environment{boost::this_process::environment()};
    auto value{environment[name]};
    if (value.empty()) return std::nullopt;
    return value.to_string();
}

std::optional<std::string> get_default_storage_path() {
    // Are we dockerized ?
    if (auto value{get("XDG_DATA_HOME")}; value.has_value()) {
        return *value;
    }
#ifdef _WIN32
    return get("LOCALAPPDATA");  // Non roaming
#else
    return get("HOME");
#endif
}

void set(const std::string& name, const std::string& value) {
    auto environment{boost::this_process::environment()};
    environment[name] = value;
}

}  // namespace znode::env