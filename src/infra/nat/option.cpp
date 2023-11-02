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

#include "option.hpp"

#include <boost/algorithm/string/predicate.hpp>

namespace znode::nat {

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
}  // namespace znode::nat