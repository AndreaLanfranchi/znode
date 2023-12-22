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

#include <optional>

#include <CLI/CLI.hpp>

#include <core/common/misc.hpp>

#include <infra/common/log.hpp>
#include <infra/common/settings.hpp>

namespace znode::cmd {

//! \brief Parses command line arguments for node instance
void parse_node_command_line(CLI::App& cli, int argc, char* argv[], AppSettings& settings);

struct IPEndPointValidator : public CLI::Validator {
    explicit IPEndPointValidator(bool allow_empty = false, uint16_t default_port = 0);
};

//! \brief Set up options to populate log settings after cli.parse()
void add_logging_options(CLI::App& cli, log::Settings& log_settings);

}  // namespace znode::cmd
