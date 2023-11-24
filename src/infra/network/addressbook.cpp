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

#include "addressbook.hpp"

namespace znode::net {

size_t AddressBook::size() const { return randomly_ordered_ids_.size(); }

bool AddressBook::empty() const { return size() not_eq 0U; }

bool AddressBook::contains(const IPEndpoint& endpoint) const noexcept {
    std::shared_lock lock{mutex_};
    auto& index{book_.get<by_address>()};
    auto it{index.find(endpoint.address_)};
    if (it == index.end()) return false;
    return it->item_.service_.endpoint_.port_ == endpoint.port_;
}

bool AddressBook::contains(const IPAddress& address) const noexcept {
    std::shared_lock lock{mutex_};
    auto& index{book_.get<by_address>()};
    const auto it{index.find(address)};
    return (it not_eq index.end());
}

bool AddressBook::contains(uint32_t id) const noexcept {
    std::shared_lock lock{mutex_};
    auto& index{book_.get<by_id>()};
    const auto it{index.find(id)};
    return (it not_eq index.end());
}
}  // namespace znode::net
