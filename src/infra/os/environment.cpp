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