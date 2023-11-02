/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>

#include <CLI/CLI.hpp>

#include <core/common/misc.hpp>

#include <infra/common/log.hpp>

#include <node/common/settings.hpp>

namespace znode::cmd {

//! \brief Parses command line arguments for node instance
void parse_node_command_line(CLI::App& cli, int argc, char* argv[], AppSettings& settings);

struct IPEndPointValidator : public CLI::Validator {
    explicit IPEndPointValidator(bool allow_empty = false, uint16_t default_port = 0);
};

//! \brief Set up options to populate log settings after cli.parse()
void add_logging_options(CLI::App& cli, log::Settings& log_settings);

}  // namespace znode::cmd
