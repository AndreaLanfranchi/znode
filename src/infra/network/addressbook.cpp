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

size_t AddressBook::size() const { return book_.size(); }

bool AddressBook::empty() const { return size() == 0U; }

bool AddressBook::contains(const NodeService& service) const noexcept { return contains(service.endpoint_); }

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

bool AddressBook::upsert(const NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;

    if (not service.endpoint_.address_.is_routable()) {
        throw std::invalid_argument("Address is not routable");
    }

    // Do not apply penalty if the source is the same as the service
    if (source == service.endpoint_.address_) {
        time_penalty = 0s;
    }

    bool ret{false};
//    std::unique_lock lock{mutex_};
//    auto& index{book_.get<by_address>()};
//    auto it{index.find(service.endpoint_.address_)};
//    if (it == index.end()) {
//        // Insert new item
//        book_entry new_entry{.address_ = service.endpoint_.address_,
//                             .id_ = last_id_.fetch_add(1U),
//                             .item_ = NodeServiceInfo(service, source)};
//        randomly_ordered_ids_.push_back(new_entry.id_);
//        std::tie(it, ret) = index.insert(new_entry);
//        auto [it2, inserted]{index.insert(new_entry)};
//        ASSERT(inserted);
//    } else {
//        // Update existing item
//        entry = &(*it);
//
//        // Periodically update time seen
//        const bool currently_online{NodeClock::now() - service.time_ < 24h};
//        const auto update_interval{currently_online ? 1h : 24h};
//        if (entry->item_.service_.time_ < (service.time_ - update_interval - time_penalty)) {
//            entry->item_.service_.time_ = std::max(NodeSeconds{0s}, service.time_ - time_penalty);
//        }
//        entry->item_.service_.services_ |= service.services_;
//    }

    return ret;
}

}  // namespace znode::net
