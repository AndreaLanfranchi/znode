/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <deque>

#include <catch2/catch.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/common/capped_set.hpp>

namespace zen {
TEST_CASE("Capped Set", "[memory]") {
    static const std::size_t kContainerSize{5'000};
    CappedSet<int> mrset(kContainerSize);
    CHECK(mrset.capacity() == kContainerSize);

    // Run 10 tests
    for (int test_num{0}; test_num < 10; ++test_num) {
        mrset.clear();

        // A deque + set to simulate the mruset.
        std::deque<int> rep;
        std::unordered_set<int> all;

        // Insert 1000 random integers below 1500.
        for (int i{0}; i < 1'000; ++i) {
            const int number = std::rand() % 1'500;
            mrset.insert(number);
            if (all.insert(number).second) rep.push_back(number);
            if (all.size() == kContainerSize + 1) {
                all.erase(rep.front());
                rep.pop_front();
            }

            // Do a full comparison between CappedSet and the simulated mru every 1000 and every 5001 elements.
            if (i % 100 == 0 || i % 501 == 0) {
                CappedSet<int> mrset_copy = mrset;  // Also try making a copy
                REQUIRE(mrset == mrset_copy);       // Which should be equal

                // Check all elements in rep are in both Mrsets
                for (const auto item : rep) {
                    REQUIRE(mrset.contains(item));
                    REQUIRE(mrset_copy.contains(item));
                }

                // Check all items in mrset are also in all
                for (const auto item : mrset) {
                    REQUIRE(all.contains(item));
                }

                // Check all items in mrset_copy are also in all
                for (const auto item : mrset_copy) {
                    REQUIRE(all.contains(item));
                }
            }
        }
    }

    // Any number above 1500 should not exist
    REQUIRE_FALSE(mrset.contains(2300));
    REQUIRE_FALSE(mrset.contains(1510));

    // Any number already present should not be inserted again
    for (const auto item : mrset) {
        REQUIRE_FALSE(mrset.insert(item).second);
    }

    // Emptyness
    CHECK_FALSE(mrset.empty());
    mrset.clear();
    CHECK(mrset.empty());
}
}  // namespace zen
