/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <CLI/CLI.hpp>
#include <zen/buildinfo.h>

#include <zen/core/common/misc.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/common/settings.hpp>

namespace zen::cmd {

struct Settings {
    NodeSettings node_settings;
    log::Settings log_settings;
};

//! \brief Parses command line arguments for node instance
void parse_node_command_line(CLI::App& cli, int argc, char* argv[], Settings& settings);

//! \brief Ensure database is ready to take off and consistent with command line arguments
void run_preflight_checklist(NodeSettings& node_settings, bool init_if_empty = true);

//! Assemble the full node name using the Cable build information
std::string get_node_name_from_build_info(const buildinfo* build_info);

struct HumanSizeParserValidator : public CLI::Validator {
    template <typename T>
    explicit HumanSizeParserValidator(T min, std::optional<T> max = std::nullopt) {
        std::stringstream out;
        out << " in [" << min << " - " << (max.has_value() ? max.value() : "inf") << "]";
        description(out.str());

        func_ = [min, max](const std::string& value) -> std::string {
            auto parsed_size{parse_human_bytes(value)};
            if (!parsed_size) {
                return std::string("Value " + value + " is not a parseable size");
            }
            const auto min_size{*parse_human_bytes(min)};
            const auto max_size{max.has_value() ? *parse_human_bytes(max.value()) : UINT64_MAX};
            if (*parsed_size < min_size || *parsed_size > max_size) {
                return "Value " + value + " not in range " + min + " to " + (max.has_value() ? max.value() : "∞");
            }
            return {};
        };
    }
};

struct IPEndPointValidator : public CLI::Validator {
    explicit IPEndPointValidator(bool allow_empty = false);
};

//! \brief Set up options to populate log settings after cli.parse()
void add_logging_options(CLI::App& cli, log::Settings& log_settings);

}  // namespace zen::cmd
