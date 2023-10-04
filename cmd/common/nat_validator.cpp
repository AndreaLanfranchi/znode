/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "nat_validator.hpp"

#include <boost/algorithm/string/predicate.hpp>

namespace zenpp::cmd::common {

NatOptionValidator::NatOptionValidator() {
    description(
        "Network address detection logic (none|stun|ip:<IP>)\n"
        "- none         no NAT, use the local IP address as public\n"
        "- auto         detect the public IP address using ipify.org (default)"
        "- 1.2.3.4      use manually provided IPv4/IPv6 address as public");
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
}  // namespace zenpp::cmd::common
