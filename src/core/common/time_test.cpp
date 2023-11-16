/*
   Copyright (c) 2009-2010 Satoshi Nakamoto
   Copyright (c) 2009-2022 The Bitcoin Core developers
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

#include <catch2/catch.hpp>

#include <core/common/time.hpp>

namespace znode {

TEST_CASE("format_ISO8601","[util]") {
    CHECK(format_ISO8601(0) == "1970-01-01T00:00:00Z");
    CHECK(format_ISO8601(0, false) == "1970-01-01");
    CHECK(format_ISO8601(1234567890) == "2009-02-13T23:31:30Z");
    CHECK(format_ISO8601(1234567890, false) == "2009-02-13");
    CHECK(format_ISO8601(1234567890, true) == "2009-02-13T23:31:30Z");
    CHECK(format_ISO8601(1317425777, true) == "2011-09-30T23:36:17Z");
}
} // namespace znode
