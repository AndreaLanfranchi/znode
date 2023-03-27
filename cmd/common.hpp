/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <CLI/CLI.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/common/settings.hpp>

namespace zen::cmd {

struct CoreSettings {
    zen::NodeSettings node_settings;
    zen::log::Settings log_settings;
};

}  // namespace zen::cmd
