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

#include <absl/strings/str_cat.h>
#include <gsl/gsl_util>

#include <core/crypto/md.hpp>

#include <infra/common/log.hpp>
#include <infra/common/stopwatch.hpp>

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

bool AddressBook::add_new(NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty) {
    std::unique_lock lock{mutex_};
    try {
        return add_new_impl(service, source, time_penalty);
    } catch (const std::invalid_argument&) {
        log::Warning("Address Book", {"invalid", service.endpoint_.to_string(), "from", source.to_string()})
            << "Discarded ...";
    }
    return false;
}

bool AddressBook::add_new(std::vector<NodeService>& services, const IPAddress& source,
                          std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;
    NodeSeconds now{Now<NodeSeconds>()};
    const auto services_size{services.size()};

    uint32_t added_count{0U};
    StopWatch sw(/*auto_start=*/true);
    std::unique_lock lock{mutex_};
    for (auto it{services.begin()}; it not_eq services.end();) {
        try {
            // Only add nodes that have the network service bit set - otherwise they are not useful
            if (not(it->services_ bitand static_cast<uint64_t>(NodeServicesType::kNodeNetwork))) {
                it = services.erase(it);
                continue;
            }

            // Adjust martian dates
            // TODO: Do we care to handle advertisements of node which have a time far in the past ? (say more than 3
            // months)
            if (it->time_ < NodeSeconds{NodeService::kTimeInit} or it->time_ > now + 10min) {
                it->time_ = now - std::chrono::days(5);
            }

            if (add_new_impl(*it, source, time_penalty)) {
                ++added_count;
                ++it;
            } else {
                // Don't bother to relay
                it = services.erase(it);
            }
        } catch (const std::invalid_argument&) {
            log::Warning("Address Book", {"invalid", it->endpoint_.to_string(), "from", source.to_string()})
                << "Discarded ...";
            it = services.erase(it);
        }
    }

    std::ignore = log::Info("Address Book",
                            {"processed", std::to_string(services_size), "in", StopWatch::format(sw.since_start()),
                             "additions", std::to_string(added_count), "buckets new/tried",
                             absl::StrCat(new_entries_size_.load(), "/", tried_entries_size_.load())});

    return (added_count > 0U);
}

bool AddressBook::add_new_impl(NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;

    if (not service.endpoint_.address_.is_routable()) {
        throw std::invalid_argument("Address is not routable");
    }

    // Do not apply penalty if the source is the same as the service
    // i.e. remote self advertisement
    if (source == service.endpoint_.address_) {
        time_penalty = 0s;
    }

    auto [book_entry, book_entry_id]{find_entry(service)};
    bool is_new_entry{book_entry == nullptr};
    if (not is_new_entry) {
        // Periodically update time seen
        const bool currently_online{NodeClock::now() - service.time_ < 24h};
        const auto update_interval{currently_online ? 1h : 24h};
        if (book_entry->service_.time_ < (service.time_ - update_interval - time_penalty)) {
            book_entry->service_.time_ = std::max(NodeSeconds{0s}, service.time_ - time_penalty);
        }
        book_entry->service_.services_ |= service.services_;

        // Do not update when:
        // 1. The provided service has a seen date older than the one in the address book
        // 2. The entry is already in the tried bucket
        // 3. The entry has already been referenced kMaxNewBucketReferences times
        if (service.time_ <= book_entry->service_.time_ or book_entry->in_tried_bucket_ or
            book_entry->new_references_count_ == kMaxNewBucketReferences) {
            return false;
        }

        // Stochastic test : previous new_references_count_ == N: 2^N times harder to increase it
        uint32_t factor{1U << book_entry->new_references_count_};
        if (randomize<uint32_t>(0U, factor) not_eq 0) {
            return false;
        }

    } else {
        // Insert new item
        std::tie(book_entry, book_entry_id) = emplace_new_entry(service, source);
        ASSERT(book_entry not_eq nullptr and book_entry_id not_eq 0);  // Must be inserted
        book_entry->service_.time_ = std::max(NodeSeconds{0s}, service.time_ - time_penalty);
    }

    // Obtain coordinates of the bucket and position in the bucket
    auto [bucket_num, bucket_pos]{compute_new_bucket_coordinates(*book_entry, source)};

    // We can insert the entry in the new bucket if:
    // 1. The slot is empty
    // 2. The slot is occupied by an entry that is either bad or is present in
    //    that very slot only (i.e. it has not been referenced by any other bucket's slot)
    const auto existing_id{new_buckets_[bucket_num][bucket_pos]};
    bool free_slot{existing_id == 0U /* empty bucket */};
    if (not free_slot and existing_id not_eq book_entry_id) {
        const auto it{map_id_to_serviceinfo_.find(existing_id)};
        ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found or else the data structures are inconsistent
        if (it->second.is_bad() or (it->second.new_references_count_ > 1U and book_entry->new_references_count_ == 0)) {
            free_slot = true;  // Will overwrite the bad entry
        }
    }
    if (free_slot) {
        if (existing_id not_eq 0U) clear_new_bucket(bucket_num, bucket_pos);
        new_buckets_[bucket_num][bucket_pos] = book_entry_id;
        ++book_entry->new_references_count_;
    } else {
        // Erase the item which could not find a slot
        if (book_entry->new_references_count_ == 0U) {
            erase_new_entry(book_entry_id);
        }
    }

    return free_slot;
}

std::pair<NodeServiceInfo*, uint32_t> AddressBook::emplace_new_entry(const NodeService& service,
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
    ASSERT((it2->second.service_.endpoint_ ==
            service.endpoint_));  // Must be the same (or else the data structures are inconsistent)
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

std::pair<uint32_t, uint32_t> AddressBook::compute_new_bucket_coordinates(const NodeServiceInfo& service,
                                                                          const IPAddress& source) const noexcept {
    const ByteView key_view{key_.data(), key_.size()};

    const auto source_group{compute_group(source)};
    const auto service_group{compute_group(service.service_.endpoint_.address_)};

    crypto::SipHash24 hasher{key_view};
    hasher.update(service_group);
    hasher.update(source_group);
    const auto hash1{endian::load_little_u64(hasher.finalize().data())};

    hasher.init(key_view);
    hasher.update(source_group);
    hasher.update(hash1 % kNewBucketsPerSourceGroup);
    const auto hash2{endian::load_little_u64(hasher.finalize().data())};

    auto bucket_num{gsl::narrow_cast<uint32_t>(hash2 % kNewBucketsCount)};

    hasher.init(key_view);
    hasher.update('N');
    hasher.update(bucket_num);
    hasher.update(service.service_.endpoint_.to_bytes());
    const auto hash3{endian::load_little_u64(hasher.finalize().data())};

    auto bucket_pos{gsl::narrow_cast<uint32_t>(hash3 % kBucketSize)};

    return {bucket_num, bucket_pos};
}

std::pair</* bucket_num */ uint32_t, /* bucket_pos */ uint32_t> AddressBook::compute_tried_bucket_coordinates(
    const NodeServiceInfo& service) const noexcept {
    const ByteView key_view{key_.data(), key_.size()};
    const auto service_group{compute_group(service.service_.endpoint_.address_)};
    const auto service_key{service.service_.endpoint_.to_bytes()};

    crypto::SipHash24 hasher{key_view};
    hasher.update(service_key);
    const auto hash1{endian::load_little_u64(hasher.finalize().data())};

    hasher.init(key_view);
    hasher.update(service_group);
    hasher.update(hash1 % kTriedBucketsPerGroup);
    const auto hash2{endian::load_little_u64(hasher.finalize().data())};

    auto bucket_num{gsl::narrow_cast<uint32_t>(hash2 % kNewBucketsCount)};

    hasher.init(key_view);
    hasher.update('T');
    hasher.update(bucket_num);
    hasher.update(service.service_.endpoint_.to_bytes());
    const auto hash3{endian::load_little_u64(hasher.finalize().data())};

    auto bucket_pos{gsl::narrow_cast<uint32_t>(hash3 % kBucketSize)};
    return {bucket_num, bucket_pos};
}

Bytes AddressBook::compute_group(const znode::net::IPAddress& address) const noexcept {
    ASSERT(address.is_routable());
    const auto address_type{address.get_type()};
    Bytes ret{1, static_cast<uint8_t>(address_type)};
    switch (address_type) {
        using enum IPAddressType;
        case kIPv4: {
            const auto subnet{IPSubNet::calculate_subnet_base_address(address, kIPv4SubnetGroupsPrefix)};
            const auto subnet_bytes{subnet.value()->to_v4().to_bytes()};
            ret.append(subnet_bytes.begin(), subnet_bytes.end());
        } break;
        case kIPv6: {
            const auto subnet{IPSubNet::calculate_subnet_base_address(address, kIPv6SubnetGroupsPrefix)};
            const auto subnet_bytes{subnet.value()->to_v6().to_bytes()};
            ret.append(subnet_bytes.begin(), subnet_bytes.end());
        } break;
        default:
            ASSERT(false);  // Should never happen
    }
    return ret;
}
}  // namespace znode::net
