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

#include <deque>

#include <catch2/catch.hpp>

#include <core/common/base.hpp>
#include <core/common/capped_set.hpp>

namespace znode {
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
}  // namespace znode
