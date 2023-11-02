/*
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
