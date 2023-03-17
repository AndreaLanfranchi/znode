/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/common/secure_bytes.hpp>

namespace zen {
TEST_CASE("Secure Bytes", "[memory]") {
    intptr_t ptr{0};
    {
        SecureBytes secure_bytes(4_KiB, '\0');
        ptr = reinterpret_cast<intptr_t>(&secure_bytes);
        secure_bytes[0] = 'a';
        secure_bytes[1] = 'b';
        secure_bytes[2] = 'c';
        CHECK_FALSE(LockedPagesManager::instance().empty());
    }
    CHECK(LockedPagesManager::instance().empty());
    CHECK_FALSE(LockedPagesManager::instance().contains(static_cast<size_t>(ptr)));
}
}  // namespace zen
