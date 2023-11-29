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

bool AddressBook::empty() const { return size() == 0U; }

bool AddressBook::contains(const NodeService& service) const noexcept { return contains(service.endpoint_); }

bool AddressBook::contains(const IPEndpoint& endpoint) const noexcept {
    std::shared_lock lock{mutex_};
    return map_endpoint_to_id_.contains(endpoint);
}

bool AddressBook::contains(uint32_t id) const noexcept {
    std::shared_lock lock{mutex_};
    return map_id_to_serviceinfo_.contains(id);
}

bool AddressBook::upsert(const NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;

    if (not service.endpoint_.address_.is_routable()) {
        throw std::invalid_argument("Address is not routable");
    }

    // Do not apply penalty if the source is the same as the service
    // i.e. remote self advertisement
    if (source == service.endpoint_.address_) {
        time_penalty = 0s;
    }

    std::unique_lock lock{mutex_};
    NodeServiceInfo* entry{nullptr};
    uint32_t entry_id{0U};
    std::tie(entry, entry_id) = find_entry(service);

    if (entry not_eq nullptr) {
        // Periodically update time seen
        const bool currently_online{NodeClock::now() - service.time_ < 24h};
        const auto update_interval{currently_online ? 1h : 24h};
        if (entry->service_.time_ < (service.time_ - update_interval - time_penalty)) {
            entry->service_.time_ = std::max(NodeSeconds{0s}, service.time_ - time_penalty);
        }
        entry->service_.services_ |= service.services_;

        // Do not update when:
        // 1. The provided service has a seen date older than the one in the address book
        // 2. The entry is already in the tried bucket
        // 3. The entry has already been referenced kMaxNewBucketReferences times
        if (service.time_ <= entry->service_.time_ or entry->in_tried_bucket_ or
            entry->new_references_count_ == kMaxNewBucketReferences) {
            return false;
        }

        // Stochastic test : previous new_references_count_ == N: 2^N times harder to increase it
        uint32_t factor{1U << entry->new_references_count_};
        if (factor > 1U && randomize(0, factor - 1) not_eq 0) {
            return false;
        }

    } else {
        // Insert new item
        std::tie(entry, entry_id) = insert_new_entry(service, source);
        ASSERT(entry not_eq nullptr and entry_id not_eq 0);  // Must be inserted
        entry->service_.time_ = std::max(NodeSeconds{0s}, service.time_ - time_penalty);
    }

    // TODO obtain coordinates of the bucket and position in the bucket
    uint32_t bucket_num{0};
    uint32_t bucket_pos{0};

    // We can insert the entry in the new bucket if:
    // 1. The slot is empty
    // 2. The slot is occupied by an entry that is either bad or is present in
    //    that very slot only (i.e. it has not been referenced by any other bucket's slot)
    const auto existing_id{new_buckets_[bucket_num][bucket_pos]};
    bool free_slot{existing_id == 0U /* empty bucket */};
    if (not free_slot and existing_id not_eq entry_id) {
        const auto it{map_id_to_serviceinfo_.find(existing_id)};
        ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found
        if (it->second.is_bad() or (it->second.new_references_count_ > 1U and entry->new_references_count_ == 0)) {
            // Free the slot

            free_slot = true;
        }
    }
}

std::pair<NodeServiceInfo*, uint32_t> AddressBook::insert_new_entry(const NodeService& service,
                                                                    const IPAddress& source) noexcept {
    uint32_t new_id{last_used_id_.fetch_add(1U)};
    auto [it, inserted]{map_id_to_serviceinfo_.try_emplace(new_id, NodeServiceInfo(service, source))};
    ASSERT(inserted);  // Must be inserted (new_id is unique)
    auto new_entry{&(it->second)};
    randomly_ordered_ids_.push_back(new_id);
    new_entry->random_pos_ = static_cast<uint32_t>(randomly_ordered_ids_.size() - 1U);
    ASSERT(map_endpoint_to_id_.insert({service.endpoint_, new_id}).second);  // Must be inserted

    ++new_entries_size_;
    return {new_entry, new_id};
}

void AddressBook::erase_new_entry(uint32_t id) noexcept {
    if (id == 0U) return;  // Cannot erase non-existent entry
    auto it{map_id_to_serviceinfo_.find(id)};
    ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found
    ASSERT(it->second.new_references_count_ == 0U);  // Must not be referenced by any "new" bucket
    ASSERT(not it->second.in_tried_bucket_);         // Must not be in the tried bucket
    ASSERT(not randomly_ordered_ids_.empty());       // Must not be empty or else the data structures are inconsistent
    swap_randomly_ordered_ids(it->second.random_pos_, static_cast<uint32_t>(randomly_ordered_ids_.size() - 1U));
    ASSERT(randomly_ordered_ids_.back() == id);  // Must have become the last element
    randomly_ordered_ids_.pop_back();
    std::ignore = map_endpoint_to_id_.erase(it->second.service_.endpoint_);
    std::ignore = map_id_to_serviceinfo_.erase(it);
    --new_entries_size_;
}

void AddressBook::clear_new_bucket(uint32_t bucket_num, uint32_t bucket_pos) noexcept {
    // Remove the reference from "new" buckets
    if (empty()) return;
    if (bucket_num >= kNewBucketsCount or bucket_pos >= kBucketSize) return;  // TODO Or throw?
    const auto id{new_buckets_[bucket_num][bucket_pos]};
    if (id == 0U) return;  // Empty slot already
    auto it{map_id_to_serviceinfo_.find(id)};
    ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found
    ASSERT(it->second.new_references_count_ > 0U);   // Must be referenced at least once if it is in new
    --it->second.new_references_count_;
    new_buckets_[bucket_num][bucket_pos] = 0U;
    if (it->second.new_references_count_ > 0U or it->second.in_tried_bucket_) return;

    // Remove the service from the address book entirely
    erase_new_entry(id);
}

std::pair<NodeServiceInfo*, /*id*/ uint32_t> AddressBook::find_entry(const NodeService& service) {
    const auto it1{map_endpoint_to_id_.find(service.endpoint_)};
    if (it1 == map_endpoint_to_id_.end()) {
        return {nullptr, 0U};
    }

    auto id{it1->second};
    auto it2{map_id_to_serviceinfo_.find(id)};
    ASSERT(it2 != map_id_to_serviceinfo_.end());  // Must be found or else the data structures are inconsistent
    return {&(it2->second), id};
}

void AddressBook::swap_randomly_ordered_ids(uint32_t i, uint32_t j) noexcept {
    if (i == j) return;
    ASSERT(i < randomly_ordered_ids_.size() and j < randomly_ordered_ids_.size());  // Must be valid indices

    auto id_at_i{randomly_ordered_ids_[i]};
    auto id_at_j{randomly_ordered_ids_[j]};
    const auto it1{map_id_to_serviceinfo_.find(id_at_i)};
    const auto it2{map_id_to_serviceinfo_.find(id_at_j)};
    ASSERT(it1 not_eq map_id_to_serviceinfo_.end());  // Must be found
    ASSERT(it2 not_eq map_id_to_serviceinfo_.end());  // Must be found

    // Swap the references
    it1->second.random_pos_ = j;
    it2->second.random_pos_ = i;

    // Swap the ids
    randomly_ordered_ids_[i] = id_at_j;
    randomly_ordered_ids_[j] = id_at_i;
}

}  // namespace znode::net
