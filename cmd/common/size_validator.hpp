/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <limits>
#include <optional>

#include <CLI/CLI.hpp>

#include <core/common/misc.hpp>

namespace zenpp::cmd::common {

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
}  // namespace zenpp::cmd::common
