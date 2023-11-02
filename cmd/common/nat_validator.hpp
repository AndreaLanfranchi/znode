/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <CLI/CLI.hpp>

#include <infra/nat/option.hpp>

namespace znode::cmd::common {
struct NatOptionValidator : public CLI::Validator {
    explicit NatOptionValidator();
};
} // namespace znode::cmd::common