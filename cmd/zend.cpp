/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <stdexcept>

#include <CLI/CLI.hpp>
#include <zen/buildinfo.h>

#include <zen/core/common/memory.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/common/stopwatch.hpp>
#include <zen/node/concurrency/ossignals.hpp>

using namespace zen;
using namespace std::chrono;

int main(int argc, char* argv[]) {

    CLI::App cli("Zen node");
    cli.get_formatter()->column_width(50);


    Ossignals::init();

}