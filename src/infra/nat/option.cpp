/*
   Copyright 2023 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "option.hpp"

#include <boost/algorithm/string/predicate.hpp>

namespace zenpp::nat {

bool lexical_cast(const std::string& input, Option& value) {
    if (input.empty()) {
        return true;  // default value
    }
    if (boost::algorithm::iequals(input, "none")) {
        value.type_ = NatType::kNone;
        return true;
    }
    if (boost::algorithm::iequals(input, "auto")) {
        value.type_ = NatType::kAuto;
        return true;
    }

    const auto parsed_address{net::IPAddress::from_string(input)};
    if (parsed_address.has_error()) {
        std::cerr << "Value \"" << input << "\" is not a valid IP address" << std::endl;
        return false;
    }

    value.type_ = NatType::kIp;
    value.address_ = parsed_address.value();
    return true;
}
}  // namespace zenpp::nat