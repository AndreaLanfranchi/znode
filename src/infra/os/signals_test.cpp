/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <csignal>

#include <catch2/catch.hpp>

#include <infra/os/signals.hpp>

namespace znode::os {

TEST_CASE("Os Signals", "[concurrency]") {
    Signals::init();  // Enable the hooks
    bool expected_trow{false};
    try {
        std::raise(SIGINT);
        Signals::throw_if_signalled();
    } catch (const signal_exception& ex) {
        expected_trow = true;
        CHECK(ex.sig_code() == SIGINT);
    }
    REQUIRE(expected_trow);
    Signals::reset();  // Otherwise other tests get blocked
    REQUIRE_FALSE(Signals::signalled());
}
}  // namespace znode::os
