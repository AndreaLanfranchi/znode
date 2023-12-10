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

#include <catch2/catch.hpp>

#include <core/common/base.hpp>
#include <core/common/secure_bytes.hpp>

namespace znode {
TEST_CASE("Secure Bytes", "[memory]") {
    intptr_t ptr{0};
    {
        SecureBytes secure_bytes(4_KiB, 0);
        ptr = reinterpret_cast<intptr_t>(&secure_bytes);
        secure_bytes[0] = 'a';
        secure_bytes[1] = 'b';
        secure_bytes[2] = 'c';
        CHECK_FALSE(LockedPagesManager::instance().empty());
    }
    CHECK(LockedPagesManager::instance().empty());
    CHECK_FALSE(LockedPagesManager::instance().contains(static_cast<size_t>(ptr)));
}
}  // namespace znode
