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

#include "nat_validator.hpp"

#include <boost/algorithm/string/predicate.hpp>

namespace znode::cmd::common {

NatOptionValidator::NatOptionValidator() {
    description(
        "Network address translation detection logic (none|auto|ip)\n"
        "\t- none         no NAT, use the local IP address as public\n"
        "\t- auto         detect the public IP address using ipify.org (default)\n"
        "\t- 1.2.3.4      use manually provided IPv4/IPv6 address as public\n");
    func_ = [](std::string& value) -> std::string {
        if (value.empty()) {
            value = "auto";
            return {};
        }
        if (boost::algorithm::iequals(value, "auto")) {
            value = "auto";
            return {};
        }
        if (boost::algorithm::iequals(value, "stun")) {
            value = "stun";
            return {};
        }

        const auto parsed_address{net::IPAddress::from_string(value)};
        if (parsed_address.has_error()) {
            return "Value \"" + value + "\" is not a valid IP address";
        }
        return {};
    };
}
}  // namespace znode::cmd::common
