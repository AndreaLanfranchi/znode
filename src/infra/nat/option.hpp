/*
   Copyright 2023 The Silkworm Authors
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

#pragma once
#include <optional>
#include <string>

#include <infra/network/addresses.hpp>

namespace znode::nat {

enum class NatType {
    kNone,  // No network address translation: local IP as public IP
    kAuto,  // Detect public IP address using IPify.org
    kIp     // Use provided IP address as public IP
};

struct Option {
    NatType type_{NatType::kAuto};
    net::IPAddress address_{};
};

//! \brief Used by CLI to convert a string to a NAT Option
bool lexical_cast(const std::string& input, Option& value);

}  // namespace znode::nat