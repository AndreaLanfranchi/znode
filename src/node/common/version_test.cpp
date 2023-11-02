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

#include <catch2/catch.hpp>

#include <node/common/version.hpp>

namespace znode {
TEST_CASE("Versions", "[misc]") {
    const Version ver0{};
    const Version ver1{0, 0, 1};
    REQUIRE(ver0 < ver1);
    REQUIRE(ver1 > ver0);
    REQUIRE(ver0.to_string() == "0.0.0");
    REQUIRE(ver1.to_string() == "0.0.1");
}
}  // namespace znode
