/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <csignal>

#include <catch2/catch.hpp>

#include <app/concurrency/ossignals.hpp>

namespace zenpp {

TEST_CASE("Os Signals", "[concurrency]") {
    Ossignals::init();  // Enable the hooks
    bool expected_trow{false};
    try {
        std::raise(SIGINT);
        Ossignals::throw_if_signalled();
    } catch (const os_signal_exception& ex) {
        expected_trow = true;
        CHECK(ex.sig_code() == SIGINT);
    }
    REQUIRE(expected_trow);
    Ossignals::reset();  // Otherwise other tests get blocked
}
}  // namespace zenpp
