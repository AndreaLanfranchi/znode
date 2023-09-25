/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>

#include <CLI/CLI.hpp>

#include <core/common/misc.hpp>

#include <infra/common/log.hpp>

#include <node/common/settings.hpp>

namespace zenpp::cmd {

//! \brief Parses command line arguments for node instance
void parse_node_command_line(CLI::App& cli, int argc, char* argv[], AppSettings& settings);

struct HumanSizeParserValidator : public CLI::Validator {
    template <typename T>
    explicit HumanSizeParserValidator(T min, std::optional<T> max = std::nullopt) {
        std::stringstream out;
        out << " in [" << min << " - " << (max.has_value() ? max.value() : "inf") << "]";
        description(out.str());

        func_ = [min, max](const std::string& value) -> std::string {
            auto parsed_size{parse_human_bytes(value)};
            if (not parsed_size) {
                return std::string("Value " + value + " is not a parseable size");
            }
            const auto min_size{*parse_human_bytes(min)};
            const auto max_size{max.has_value() ? *parse_human_bytes(max.value()) : UINT64_MAX};
            if (*parsed_size < min_size or *parsed_size > max_size) {
                return "Value " + value + " not in range " + min + " to " + (max.has_value() ? max.value() : "inf");
            }
            return {};
        };
    }
};

struct TimeZoneValidator : public CLI::Validator {
    explicit TimeZoneValidator(bool allow_empty = false);
};

struct IPEndPointValidator : public CLI::Validator {
    explicit IPEndPointValidator(bool allow_empty = false, uint16_t default_port = 0);
};

//! \brief Set up options to populate log settings after cli.parse()
void add_logging_options(CLI::App& cli, log::Settings& log_settings);

}  // namespace zenpp::cmd
