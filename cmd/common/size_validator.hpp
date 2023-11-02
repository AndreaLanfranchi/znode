/*
   Copyright 2022 The Silkworm Authors
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

#include <limits>
#include <optional>

#include <CLI/CLI.hpp>

#include <core/common/misc.hpp>

namespace znode::cmd::common {

struct SizeValidator : public CLI::Validator {
    template <typename T>
    explicit SizeValidator(T min, std::optional<T> max = std::nullopt,
                           const std::string& validator_name = std::string{})
        : CLI::Validator{validator_name} {
        std::stringstream out;
        out << " in [" << min << ".."
            << (max.has_value() ? max.value() : ("max<" + std::string(typeid(T).name()) + ">")) << "]";
        description(out.str());

        func_ = [min, max](const std::string& value) -> std::string {
            const auto parsed_size{parse_human_bytes(value)};
            if (parsed_size.has_error()) {
                return std::string("Value " + value + " is not a parseable size");
            }
            const auto min_size{parse_human_bytes(min).value()};
            const auto max_size{max.has_value() ? parse_human_bytes(max.value()).value()
                                                : std::numeric_limits<size_t>::max()};
            if (parsed_size.value() < min_size or parsed_size.value() > max_size) {
                return "Value \"" + value + "\" not in range [" + min + ".." +
                       (max.has_value() ? max.value() : ("max<" + std::string(typeid(T).name()) + ">"));
            }
            return {};
        };
    }
};
}  // namespace znode::cmd::common
