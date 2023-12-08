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
#include <unordered_set>

#include <catch2/catch.hpp>

#include <core/common/lru_set.hpp>
#include <core/common/random.hpp>

namespace znode {
TEST_CASE("Last Recently Used Set", "[memory]") {
    const int kContainerSize{10};
    LruSet<int> lruset(kContainerSize);
    CHECK(lruset.max_size() == kContainerSize);

    // Fill with 10 items
    for (int i{0}; i < kContainerSize; ++i) {
        CHECK(lruset.insert(i));
    }

    CHECK(lruset.size() == kContainerSize);
    CHECK_FALSE(lruset.empty());

    // Inserting an item already present should return false
    for (int i{0}; i < kContainerSize; ++i) {
        CHECK_FALSE(lruset.insert(i));
    }

    // The item on top of the list should be the element inserted last
    CHECK(lruset.front() == kContainerSize - 1);
    CHECK(lruset.back() == 0);

    // Add another item and ensure the last item is now 1
    CHECK(lruset.insert(kContainerSize));
    CHECK(lruset.front() == kContainerSize);
    CHECK(lruset.back() == 1);
    CHECK_FALSE(lruset.contains(0));
}
}  // namespace znode
