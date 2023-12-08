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

size_t AddressBook::size() const {
    std::shared_lock lock{mutex_};
    return randomly_ordered_ids_.size();
}

std::pair<uint32_t, uint32_t> AddressBook::size_by_buckets() const {
    return {new_entries_size_.load(), tried_entries_size_.load()};
}

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
    std::set<IPAddress> unique_addresses{};

    StopWatch sw(/*auto_start=*/true);
    std::unique_lock lock{mutex_};
    for (auto it{services.begin()}; it not_eq services.end();) {
        try {
            // Only add nodes that have the network service bit set - otherwise they are not useful
            if (not(it->services_ bitand static_cast<uint64_t>(NodeServicesType::kNodeNetwork))) {
                it = services.erase(it);
                continue;
            }

            // Verify remotes are not pushing duplicate addresses
            // it's a violation of the protocol
            if (!unique_addresses.insert(it->endpoint_.address_).second) {
                throw std::invalid_argument("Duplicate address");
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

    if (log::test_verbosity(log::Level::kTrace)) {
        std::ignore = log::Trace("Address Book",
                                 {"processed", std::to_string(services_size), "in", StopWatch::format(sw.since_start()),
                                  "additions", std::to_string(added_count), "buckets new/tried",
                                  absl::StrCat(new_entries_size_.load(), "/", tried_entries_size_.load())});
    }

    return (added_count > 0U);
}

bool AddressBook::set_good(const IPEndpoint& remote, NodeSeconds time) noexcept {
    std::unique_lock lock{mutex_};
    auto [entry, entry_id]{find_entry(remote)};
    if (entry == nullptr) return false;  // No such an entry
    auto& service_info{*entry};

    // Update info
    service_info.service_.time_ = time;
    service_info.last_connection_attempt_ = time;
    service_info.last_connection_success_ = time;
    service_info.connection_attempts_ = 0U;

    // Ensure is in the tried bucket
    if (!service_info.tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
}

bool AddressBook::set_failed(const znode::net::IPEndpoint& remote, znode::NodeSeconds time) noexcept {
    std::unique_lock lock{mutex_};
    auto [entry, entry_id]{find_entry(remote)};
    if (entry == nullptr) return false;  // No such an entry
    auto& service_info{*entry};

    // Update info
    service_info.last_connection_attempt_ = time;
    ++service_info.connection_attempts_;

    if (!service_info.tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
}

bool AddressBook::set_tried(const znode::net::IPEndpoint& remote, znode::NodeSeconds time) noexcept {
    std::unique_lock lock{mutex_};
    auto [entry, entry_id]{find_entry(remote)};
    if (entry == nullptr) return false;  // No such an entry
    auto& service_info{*entry};

    // Update info
    service_info.last_connection_attempt_ = time;

    if (!service_info.tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
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

    auto [book_entry, book_entry_id]{find_entry(service.endpoint_)};
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
        if (service.time_ <= book_entry->service_.time_ or book_entry->tried_ref_.has_value() or
            book_entry->new_refs_.size() == kMaxNewBucketReferences) {
            return false;
        }

        // Stochastic test : previous new_references_count_ == N: 2^N times harder to increase it
        uint32_t factor{1U << book_entry->new_refs_.size()};
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
    const auto slot_address{get_new_slot(*book_entry, source)};
    auto& slot{new_buckets_[slot_address.x][slot_address.y]};

    // We can insert the entry in the new bucket if:
    // 1. The slot is empty
    // 2. The slot is occupied by an entry that is either bad or is present in
    //    that very slot only (i.e. it has not been referenced by any other bucket's slot)
    bool is_free_slot{slot == 0U /* empty bucket */};
    if (not is_free_slot and slot not_eq book_entry_id) {
        const auto it{map_id_to_serviceinfo_.find(slot)};
        ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found or else the data structures are inconsistent
        if (it->second.is_bad() or (it->second.new_refs_.size() > 1U and book_entry->new_refs_.empty())) {
            is_free_slot = true;  // Will overwrite the bad entry
        }
    }
    if (is_free_slot) {
        if (slot not_eq 0U) clear_new_slot(slot_address);
        slot = book_entry_id;
    } else {
        // Erase the item which could not find a slot
        if (book_entry->new_refs_.empty()) {
            erase_new_entry(book_entry_id);
        }
    }

    return is_free_slot;
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
    ASSERT(it->second.new_refs_.empty());            // Must not be referenced by any "new" bucket
    ASSERT(!it->second.tried_ref_.has_value());      // Must not be in the tried bucket
    ASSERT(not randomly_ordered_ids_.empty());       // Must not be empty or else the data structures are inconsistent
    swap_randomly_ordered_ids(it->second.random_pos_, static_cast<uint32_t>(randomly_ordered_ids_.size() - 1U));
    ASSERT(randomly_ordered_ids_.back() == id);  // Must have become the last element
    randomly_ordered_ids_.pop_back();
    std::ignore = map_endpoint_to_id_.erase(it->second.service_.endpoint_);
    std::ignore = map_id_to_serviceinfo_.erase(it);
    --new_entries_size_;
}

void AddressBook::make_entry_tried(uint32_t entry_id) noexcept {
    using namespace std::chrono_literals;
    auto it{map_id_to_serviceinfo_.find(entry_id)};
    ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found
    auto& service_info{it->second};

    if (service_info.tried_ref_.has_value()) return;  // Already in the tried bucket

    // Erase all references from the "new" buckets
    new_entries_size_ -= static_cast<uint32_t>(!service_info.new_refs_.empty());
    for (auto slots_iterator{service_info.new_refs_.begin()}; slots_iterator not_eq service_info.new_refs_.end();) {
        SlotAddress slot_address(*slots_iterator);
        new_buckets_[slot_address.x][slot_address.y] = 0U;
        slots_iterator = service_info.new_refs_.erase(slots_iterator);
    }

    auto slot_address{get_tried_slot(service_info)};
    auto& slot{tried_buckets_[slot_address.x][slot_address.y]};
    if (slot not_eq 0U) {
        // Evict existing item from the tried bucket
        auto id_to_evict{slot};
        auto it2{map_id_to_serviceinfo_.find(id_to_evict)};
        ASSERT(it2 not_eq map_id_to_serviceinfo_.end());  // Must be found
        ASSERT(it2->second.tried_ref_.has_value() &&
               it2->second.tried_ref_.value() == slot_address.xy);  // Must be in the tried bucket
        it2->second.tried_ref_.reset();
        slot = 0U;
        --tried_entries_size_;

        // Re-insert the evicted item in the new bucket (must happen)
        ASSERT(add_new(it2->second.service_, it2->second.origin_, 0s));
    }

    slot = entry_id;
    ++tried_entries_size_;
    service_info.tried_ref_.emplace(slot_address.xy);
}

void AddressBook::clear_new_slot(const SlotAddress& slot_address, bool erase_entry) noexcept {
    if (empty()) return;
    if (slot_address.x >= kNewBucketsCount or slot_address.y >= kBucketSize) return;  // TODO Or throw?
    const auto id{new_buckets_[slot_address.x][slot_address.y]};
    if (id == 0U) return;  // Empty slot already
    const auto it{map_id_to_serviceinfo_.find(id)};
    ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found

    auto it2{it->second.new_refs_.find(slot_address.xy)};
    ASSERT(it2 not_eq it->second.new_refs_.end());  // Must be found
    std::ignore = it->second.new_refs_.erase(it2);
    new_buckets_[slot_address.x][slot_address.y] = 0U;
    if (!it->second.new_refs_.empty() or it->second.tried_ref_.has_value()) return;

    // Remove the service from the address book entirely
    if (erase_entry) erase_new_entry(id);
}

std::pair<NodeServiceInfo*, /*id*/ uint32_t> AddressBook::find_entry(const IPEndpoint& endpoint) {
    const auto it1{map_endpoint_to_id_.find(endpoint)};
    if (it1 == map_endpoint_to_id_.end()) {
        return {nullptr, 0U};
    }

    const auto id{it1->second};
    const auto it2{map_id_to_serviceinfo_.find(id)};
    ASSERT(it2 != map_id_to_serviceinfo_.end());  // Must be found or else the data structures are inconsistent
    ASSERT((it2->second.service_.endpoint_ ==
            endpoint));  // Must be the same (or else the data structures are inconsistent)
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

AddressBook::SlotAddress AddressBook::get_new_slot(const NodeServiceInfo& service,
                                                   const IPAddress& source) const noexcept {
    const ByteView key_view{key_.data(), key_.size()};
    const auto source_group{compute_group(source)};
    const auto service_group{compute_group(service.service_.endpoint_.address_)};

    SlotAddress ret{uint32_t(0)};

    crypto::SipHash24 hasher{key_view};
    hasher.update(service_group);
    hasher.update(source_group);
    const auto hash1{endian::load_little_u64(hasher.finalize().data())};

    hasher.init(key_view);
    hasher.update(source_group);
    hasher.update(hash1 % kNewBucketsPerSourceGroup);
    const auto hash2{endian::load_little_u64(hasher.finalize().data())};

    ret.x = gsl::narrow_cast<uint16_t>(hash2 % kNewBucketsCount);

    hasher.init(key_view);
    hasher.update('N');
    hasher.update(ret.x);
    hasher.update(service.service_.endpoint_.to_bytes());
    const auto hash3{endian::load_little_u64(hasher.finalize().data())};

    ret.y = gsl::narrow_cast<uint16_t>(hash3 % kBucketSize);

    return ret;
}

AddressBook::SlotAddress AddressBook::get_tried_slot(const NodeServiceInfo& service) const noexcept {
    const ByteView key_view{key_.data(), key_.size()};
    const auto service_group{compute_group(service.service_.endpoint_.address_)};
    const auto service_key{service.service_.endpoint_.to_bytes()};

    SlotAddress ret{uint32_t(0)};

    crypto::SipHash24 hasher{key_view};
    hasher.update(service_key);
    const auto hash1{endian::load_little_u64(hasher.finalize().data())};

    hasher.init(key_view);
    hasher.update(service_group);
    hasher.update(hash1 % kTriedBucketsPerGroup);
    const auto hash2{endian::load_little_u64(hasher.finalize().data())};

    ret.x = gsl::narrow_cast<uint16_t>(hash2 % kTriedBucketsCount);

    hasher.init(key_view);
    hasher.update('T');
    hasher.update(ret.x);
    hasher.update(service.service_.endpoint_.to_bytes());
    const auto hash3{endian::load_little_u64(hasher.finalize().data())};

    ret.y = gsl::narrow_cast<uint16_t>(hash3 % kBucketSize);

    return ret;
}

Bytes AddressBook::compute_group(const IPAddress& address) noexcept {
    ASSERT(address.is_routable());
    const auto address_type{address.get_type()};
    Bytes ret{1, static_cast<uint8_t>(address_type)};
    switch (address_type) {
        using enum IPAddressType;
        case kIPv4: {
            const auto subnet{IPSubNet::calculate_subnet_base_address(address, kIPv4SubnetGroupsPrefix)};
            const auto subnet_bytes{subnet.value().to_bytes()};
            ret.append(subnet_bytes.begin(), subnet_bytes.end());
        } break;
        case kIPv6: {
            const auto subnet{IPSubNet::calculate_subnet_base_address(address, kIPv6SubnetGroupsPrefix)};
            const auto subnet_bytes{subnet.value().to_bytes()};
            ret.append(subnet_bytes.begin(), subnet_bytes.end());
        } break;
        default:
            ASSERT(false);  // Should never happen
    }
    return ret;
}

uint32_t AddressBook::get_entry_id(SlotAddress slot, bool tried) const noexcept {
    if (tried) {
        if (slot.x >= kTriedBucketsCount or slot.y >= kBucketSize) return 0U;
        return tried_buckets_[slot.x][slot.y];
    }
    if (slot.x >= kNewBucketsCount or slot.y >= kBucketSize) return 0U;
    return new_buckets_[slot.x][slot.y];
}

std::pair<std::optional<IPEndpoint>, NodeSeconds> AddressBook::select_random(
    bool new_only, std::optional<IPAddressType> type) const noexcept {
    std::pair<std::optional<IPEndpoint>, NodeSeconds> ret{};

    std::unique_lock lock{mutex_};
    if (randomly_ordered_ids_.empty()) return ret;
    if (new_only and new_entries_size_ == 0U) return ret;

    // Determine whether to select from new or tried buckets
    bool select_from_tried{false};
    if (new_only or tried_entries_size_.load() == 0U) {
        // Select from new buckets
        select_from_tried = false;
    } else {
        if (new_entries_size_.load() == 0U) {
            // Select from tried buckets
            select_from_tried = true;
        } else {
            // Select from new or tried buckets
            select_from_tried = static_cast<bool>(randomize<uint32_t>(0U, 1U));
        }
    }

    const auto max_bucket_count{select_from_tried ? kTriedBucketsCount : kNewBucketsCount};
    const auto items_in_set{select_from_tried ? tried_entries_size_.load() : new_entries_size_.load()};
    double chance_factor{1.0};
    SlotAddress slot_address{0U};
    for (int attempt{0}; attempt < 50; ++attempt) {
        slot_address.x = randomize<uint16_t>(uint16_t(0), uint16_t(max_bucket_count - 1U));
        const auto initial_y{randomize<uint16_t>(uint16_t(0), uint16_t(kBucketSize - 1U))};
        uint16_t i{0U};
        uint32_t entry_id{0U};
        for (; i < kBucketSize; ++i) {
            slot_address.y = (initial_y + i) % kBucketSize;
            entry_id = get_entry_id(slot_address, select_from_tried);
            if (entry_id == 0U) continue;
            if (type.has_value()) {
                // TODO : Avoid this double lookup
                const auto it{map_id_to_serviceinfo_.find(entry_id)};
                ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found or data structures are inconsistent
                const auto& service_info{it->second};
                if (service_info.service_.endpoint_.address_.get_type() not_eq type.value()) continue;
            }
            break;
        }
        if (i == kBucketSize) continue;  // No entry found in this bucket pick another one
        const auto it{map_id_to_serviceinfo_.find(entry_id)};
        ASSERT(it not_eq map_id_to_serviceinfo_.end());  // Must be found or data structures are inconsistent
        const auto& service_info{it->second};
        if (randbits(30) < static_cast<uint64_t>(chance_factor * service_info.get_chance() * (1ULL << 30))) {
            ret.first = service_info.service_.endpoint_;
            ret.second = service_info.service_.time_;
        } else {
            chance_factor *= 1.2;  // Increase probability of selecting something at each iteration
        }

        // Check we do not provide a recently extracted endpoint
        if (ret.first.has_value()) {
            if (!recently_selected_.insert(ret.first.value()) && items_in_set > recently_selected_.size()) {
                ret.first.reset();
                ret.second = NodeSeconds{std::chrono::seconds(0)};
            } else {
                break;  // Ok to return this
            }
        }
    }
    return ret;
}
}  // namespace znode::net
