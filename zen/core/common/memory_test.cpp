/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/common/memory.hpp>
#include <zen/core/common/misc.hpp>

namespace zen {

TEST_CASE("Memory Usage", "[memory]") {
    size_t memory_usage_resident{get_mem_usage(true)};
    REQUIRE(memory_usage_resident > 0);
    size_t memory_usage_virtual{get_mem_usage(false)};
    REQUIRE(memory_usage_virtual > 0);
}

TEST_CASE("System Memory", "[memory]") {
    size_t sys_page_size{get_system_page_size()};
    REQUIRE(sys_page_size > 0);
    REQUIRE((sys_page_size & (sys_page_size - 1)) == 0);  // Must be power of 2
    std::cout << "Using " << zen::to_human_bytes(sys_page_size) << " memory pages" << std::endl;
}

static const void* last_lock_addr{nullptr};
static const void* last_unlock_addr{nullptr};
static size_t last_lock_len{0};
static size_t last_unlock_len{0};

class TestLocker {
  public:
    static bool lock(const void* addr, size_t len) {
        last_lock_addr = addr;
        last_lock_len = len;
        return true;
    }
    static bool unlock(const void* addr, size_t len) {
        last_unlock_addr = addr;
        last_unlock_len = len;
        return true;
    }
};

TEST_CASE("Page Locker", "[memory]") {
    const size_t kTestPageSize = 4_KiB;
    LockedPagesManagerBase<TestLocker> lpm(kTestPageSize);
    CHECK(lpm.empty());
    size_t address{1};

    // Try large number of small objects
    for (int i{0}; i < 1'000; ++i) {
        CHECK(lpm.lock_range(reinterpret_cast<void*>(address), 33));
        CHECK(lpm.contains(address));
        address += 33;
    }

    // Try small number of page-sized objects, straddling two pages
    address = kTestPageSize * 100 + 53;
    for (int i{0}; i < 100; ++i) {
        CHECK(lpm.lock_range(reinterpret_cast<void*>(address), kTestPageSize));
        CHECK(lpm.contains(address));
        address += kTestPageSize;
    }

    // Try small number of page-sized objects aligned to exactly one page
    address = kTestPageSize * 300;
    for (int i{0}; i < 100; ++i) {
        CHECK(lpm.lock_range(reinterpret_cast<void*>(address), kTestPageSize));
        CHECK(lpm.contains(address));
        address += kTestPageSize;
    }

    // One very large object, straddling pages
    CHECK(lpm.lock_range(reinterpret_cast<void*>(kTestPageSize * 600 + 1), kTestPageSize * 500));
    CHECK(last_lock_addr == reinterpret_cast<void*>(kTestPageSize * (600 + 500)));

    // One very large object, page aligned
    CHECK(lpm.lock_range(reinterpret_cast<void*>(kTestPageSize * 1200), kTestPageSize * 500 - 1));
    CHECK(last_lock_addr == reinterpret_cast<void*>(kTestPageSize * (1200 + 500 - 1)));
    CHECK(last_unlock_addr == nullptr);  // Nothing unlocked yet

    CHECK(lpm.size() == ((1000 * 33 + kTestPageSize - 1) / kTestPageSize +  // small objects
                         101 + 100 +                                        // page-sized objects
                         501 + 500));                                       // large objects

    // ... and unlock
    address = 1;
    for (int i{0}; i < 1'000; ++i) {
        CHECK(lpm.unlock_range(reinterpret_cast<void*>(address), 33));
        address += 33;
    }

    address = kTestPageSize * 100 + 53;
    for (int i{0}; i < 100; ++i) {
        CHECK(lpm.unlock_range(reinterpret_cast<void*>(address), kTestPageSize));
        address += kTestPageSize;
    }

    address = kTestPageSize * 300;
    for (int i{0}; i < 100; ++i) {
        CHECK(lpm.unlock_range(reinterpret_cast<void*>(address), kTestPageSize));
        address += kTestPageSize;
    }

    CHECK(lpm.unlock_range(reinterpret_cast<void*>(kTestPageSize * 600 + 1), kTestPageSize * 500));
    CHECK(last_unlock_addr == reinterpret_cast<void*>(kTestPageSize * (600 + 500)));
    CHECK(lpm.unlock_range(reinterpret_cast<void*>(kTestPageSize * 1200), kTestPageSize * 500 - 1));
    CHECK(last_unlock_addr == reinterpret_cast<void*>(kTestPageSize * (1200 + 500 - 1)));
    CHECK(lpm.empty());

    address = kTestPageSize;
    CHECK(lpm.lock_range(reinterpret_cast<void*>(address), kTestPageSize * 5));
    CHECK(lpm.size() == 5);
    lpm.clear();
    CHECK(lpm.empty());
    CHECK(last_unlock_addr == reinterpret_cast<void*>(kTestPageSize * (1 + 5 - 1)));
}

TEST_CASE("Lock object", "[memory]") {
    std::vector<uint64_t> vector(50, 0);
    vector[0] = 10;
    vector[1] = 10;
    for (const auto& v : vector) {
        REQUIRE(lock_object_memory(v));
    }
    CHECK_FALSE(LockedPagesManager::instance().empty());
    for (const auto& v : vector) {
        REQUIRE(unlock_object_memory(v));
    }
    REQUIRE(vector[1] == 0);
    CHECK(LockedPagesManager::instance().empty());
}

}  // namespace zen
