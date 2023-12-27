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

#include <infra/common/log.hpp>
#include <infra/common/stopwatch.hpp>
#include <infra/database/access_layer.hpp>
#include <infra/database/mdbx_tables.hpp>

namespace znode::net {

size_t AddressBook::size() const {
    std::shared_lock lock{mutex_};
    return list_index_.size();
}

std::pair<uint32_t, uint32_t> AddressBook::size_by_buckets() const {
    return {new_entries_size_.load(), tried_entries_size_.load()};
}

bool AddressBook::empty() const {
    std::shared_lock lock{mutex_};
    return list_index_.empty();
}

bool AddressBook::insert_or_update(NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty) {
    std::scoped_lock lock{mutex_};
    try {
        return insert_or_update_impl(service, source, time_penalty).second;
    } catch (const std::invalid_argument& ex) {
        log::Warning("Address Book",
                     {"invalid", service.endpoint_.to_string(), "from", source.to_string(), "reason", ex.what()})
            << "Discarded ...";
    }
    return false;
}

bool AddressBook::insert_or_update(std::vector<NodeService>& services, const IPAddress& source,
                                   std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;
    if (services.empty()) return false;
    NodeSeconds now{Now<NodeSeconds>()};
    const auto services_size{services.size()};

    uint32_t added_count{0U};
    std::set<IPEndpoint> unique_endpoints{};

    StopWatch sw(/*auto_start=*/true);
    std::scoped_lock lock{mutex_};
    for (auto it{services.begin()}; it not_eq services.end();) {
        try {
            // Only add nodes that have the network service bit set - otherwise they are not useful
            if (not(it->services_ bitand static_cast<uint64_t>(NodeServicesType::kNodeNetwork))) {
                it = services.erase(it);
                continue;
            }

            // Verify remotes are not pushing duplicate addresses
            // it's a violation of the protocol
            if (!unique_endpoints.insert(it->endpoint_).second) {
                throw std::invalid_argument("Duplicate endpoint");
            }

            // Adjust martian dates
            // TODO: Do we care to handle advertisements of node which have a time far in the past ? (say more than 3
            // months)
            if (it->time_ < NodeSeconds{NodeService::kTimeInit} or it->time_ > now + 10min) {
                it->time_ = now - std::chrono::days(5);
            }

            if (insert_or_update_impl(*it, source, time_penalty).second) {
                ++added_count;
                ++it;
            } else {
                // Don't bother to relay
                it = services.erase(it);
            }
        } catch (const std::invalid_argument& ex) {
            log::Warning("Address Book", {"invalid address", it->endpoint_.to_string(), "from", source.to_string(),
                                          "reason", ex.what()})
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

bool AddressBook::set_good(const IPEndpoint& remote, const MsgVersionPayload& version_info, NodeSeconds time) noexcept {
    std::scoped_lock lock{mutex_};
    auto [service_info, entry_id]{lookup_entry(remote)};
    if (service_info == nullptr) return false;  // No such an entry

    // Update info
    service_info->user_agent_ = version_info.user_agent_;
    service_info->service_.services_ = version_info.services_;
    service_info->service_.time_ = time;
    service_info->last_connection_attempt_ = time;
    service_info->last_connection_success_ = time;
    service_info->connection_attempts_ = 0U;

    // Ensure is in the tried bucket
    if (!service_info->tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
}

bool AddressBook::set_failed(const IPEndpoint& remote, NodeSeconds time) noexcept {
    std::scoped_lock lock{mutex_};
    auto [service_info, entry_id]{lookup_entry(remote)};
    if (service_info == nullptr) return false;  // No such an entry

    // Update info
    service_info->last_connection_attempt_ = time;
    ++service_info->connection_attempts_;

    if (!service_info->tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
}

bool AddressBook::set_tried(const IPEndpoint& remote, NodeSeconds time) noexcept {
    std::scoped_lock lock{mutex_};
    auto [entry, entry_id]{lookup_entry(remote)};
    if (entry == nullptr) return false;  // No such an entry
    auto& service_info{*entry};

    // Update info
    service_info.last_connection_attempt_ = time;

    if (!service_info.tried_ref_.has_value()) make_entry_tried(entry_id);
    return true;
}

bool AddressBook::contains(const NodeService& service) const noexcept { return contains(service.endpoint_); }

bool AddressBook::contains(const IPEndpoint& endpoint) const noexcept {
    std::shared_lock lock{mutex_};
    auto& idx{list_index_.get<by_endpoint>()};
    return idx.contains(endpoint);
}

bool AddressBook::contains(uint32_t id) const noexcept {
    std::shared_lock lock{mutex_};
    auto& idx{list_index_.get<by_id>()};
    return idx.contains(id);
}

std::pair<std::optional<IPEndpoint>, NodeSeconds> AddressBook::select_random(
    bool new_only, std::optional<IPAddressType> type) const noexcept {
    std::pair<std::optional<IPEndpoint>, NodeSeconds> ret{};

    std::scoped_lock lock{mutex_};
    if (randomly_ordered_ids_.empty()) return ret;
    if (new_only and new_entries_size_.load() == 0U) return ret;

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
            // Select from new or tried buckets with a 50% chance
            // to pick from either
            select_from_tried = static_cast<bool>(randomize<uint32_t>(0U, 1U));
        }
    }

    const auto items_in_set{select_from_tried ? tried_entries_size_.load() : new_entries_size_.load()};
    const auto& bucket{select_from_tried ? tried_buckets_ : new_buckets_};
    const auto bucket_size{bucket.size()};

    double chance_factor{1.0};
    for (int attempt{0}; attempt < 50'000; ++attempt) {
        // Pick a random item in the bucket
        const uint32_t random_item_index{static_cast<uint32_t>(randomize<size_t>(0U, bucket_size - 1))};
        const auto random_item_it{std::next(bucket.begin(), random_item_index)};
        const auto entry_id{random_item_it->second};

        NodeServiceInfo* service_info{nullptr};
        std::tie(service_info, std::ignore) = lookup_entry(entry_id);
        ASSERT(service_info not_eq nullptr);  // Must be found or else the data structures are inconsistent

        // Check type
        if (type.has_value()) {
            if (service_info->service_.endpoint_.address_.get_type() not_eq type.value()) continue;
        }
        if (randbits(30) < static_cast<uint64_t>(chance_factor * service_info->get_chance() * (1ULL << 30))) {
            ret.first = service_info->service_.endpoint_;
            ret.second = service_info->service_.time_;
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

std::vector<NodeService> AddressBook::get_random_services(uint32_t max_count,
                                                          std::optional<IPAddressType> type) noexcept {
    std::scoped_lock lock{mutex_};
    if (randomly_ordered_ids_.empty()) return {};

    size_t count{randomly_ordered_ids_.size() * kMaxGetAddrPercent / 100};
    if (max_count > 0U) {
        count = std::min(count, static_cast<size_t>(max_count));
    }

    std::set<IPEndpoint> selected_endpoints{};
    const auto now{Now<NodeSeconds>()};
    std::vector<NodeService> ret{};
    ret.reserve(count);
    auto& idx{list_index_.get<by_id>()};

    for (size_t i{0}; ret.size() < count && i < randomly_ordered_ids_.size(); ++i) {
        size_t random_index{randomize<size_t>(0U, randomly_ordered_ids_.size() - i - 1U) + i};
        swap_randomly_ordered_ids(static_cast<uint32_t>(i), static_cast<uint32_t>(random_index));
        auto it{idx.find(randomly_ordered_ids_[i])};
        ASSERT(it not_eq idx.end());  // Must be found or else the data structures are inconsistent
        const auto& service_info{*it->list_it};
        if (type.has_value() and service_info.service_.endpoint_.address_.get_type() not_eq type.value()) continue;
        if (service_info.is_bad(now)) continue;
        if (!selected_endpoints.insert(service_info.service_.endpoint_).second) continue;  // Duplicate
        ret.push_back(service_info.service_);
    }
    return ret;
}

bool AddressBook::start() noexcept {
    bool ret{Stoppable::start()};
    if (ret) {
        service_timer_.start(std::chrono::minutes(5),
                             [this](std::chrono::milliseconds& interval) { on_service_timer_expired(interval); });
    }
    return ret;
}

bool AddressBook::stop() noexcept {
    bool ret{Stoppable::stop()};
    if (ret) {
        service_timer_.stop();
    }
    return ret;
}

void AddressBook::load() {
    using namespace std::chrono_literals;
    std::scoped_lock lock{mutex_};
    StopWatch sw(/*auto_start=*/true);
    try {
        auto& env_config{app_settings_.nodedata_env_config};
        env_config.path = (*app_settings_.data_directory)[DataDirectory::kNodesName].path().string();
        env_config.create = !std::filesystem::exists(db::get_datafile_path(env_config.path));
        env_config.exclusive = true;

        log::Info("Opening database", {"path", env_config.path});
        auto node_data_env{db::open_env(env_config)};
        db::RWTxn txn(node_data_env);
        db::tables::deploy_tables(*txn, db::tables::kNodeDataTables);
        txn.commit(/*renew=*/true);

        auto key_data(db::read_config_value(*txn, "seed"));
        if (key_data && key_data.value().size() == (2 * sizeof(uint64_t))) {
            key_ = key_data.value();
        }

        db::Cursor cursor(txn, db::tables::kServices);
        log::Info("Address Book", {"action", "loading", "entries", std::to_string(cursor.size())});

        ser::SDataStream data_stream(ser::Scope::kStorage, 0);
        auto data{cursor.to_first(/*throw_notfound=*/false)};
        uint32_t entry_id{1U};

        // Load services
        while (data) {
            data_stream.clear();
            std::ignore = data_stream.write(db::from_slice(data.value));
            entry_id = endian::load_big_u32(db::from_slice(data.key).data());

            NodeServiceInfo service_info;
            const auto result{service_info.deserialize(data_stream)};
            if (!result.has_error()) {
                auto new_it{list_.emplace(list_.end(), std::move(service_info))};
                list_index_.insert({entry_id, service_info.service_.endpoint_, new_it});
            }
            data = cursor.to_next(/*throw_notfound=*/false);
        }
        last_used_id_.exchange(entry_id + 1);  // Is the last used

        // Load randomly ordered ids
        cursor.bind(txn, db::tables::kRandomOrder);
        randomly_ordered_ids_.reserve(cursor.size());
        data = cursor.to_first(/*throw_notfound=*/false);
        while (data) {
            entry_id = endian::load_big_u32(db::from_slice(data.value).data());
            randomly_ordered_ids_.push_back(entry_id);
            data = cursor.to_next(/*throw_notfound=*/false);
        }

        // Load buckets
        cursor.bind(txn, db::tables::kBuckets);
        data = cursor.to_first(/*throw_notfound=*/false);
        while (data) {
            const auto key_view{db::from_slice(data.key)};
            const auto value_view{db::from_slice(data.value)};
            const auto bucket_type{key_view[0]};
            const auto slot_address{endian::load_big_u32(key_view.data() + 1)};
            entry_id = endian::load_big_u32(value_view.data());

            NodeServiceInfo* service_info{nullptr};
            std::tie(service_info, std::ignore) = lookup_entry(entry_id);
            ASSERT(service_info != nullptr);  // Must be found or else the data structures are inconsistent

            switch (bucket_type) {
                case 'N': {
                    ASSERT(service_info->new_refs_.emplace(slot_address).second);  // Must be inserted
                    ASSERT(new_buckets_.emplace(slot_address, entry_id).second);   // Must be inserted
                    if (service_info->new_refs_.size() == 1U) ++new_entries_size_;
                } break;
                case 'T': {
                    ASSERT(service_info->tried_ref_.has_value() == false);  // Must not be in the tried bucket yet
                    service_info->tried_ref_.emplace(slot_address);
                    ASSERT(tried_buckets_.emplace(slot_address, entry_id).second);  // Must be inserted
                    ++tried_entries_size_;
                } break;
                default:
                    ASSERT(false);  // Should never happen
            }
            data = cursor.to_next(/*throw_notfound=*/false);
        }

        cursor.close();
        txn.abort();
        log::Info("Closing database", {"path", env_config.path});
        node_data_env.close();
    } catch (const std::exception& ex) {
        log::Error("Address Book", {"action", "saving", "error", ex.what()});
        return;
    }

    log::Info("Address Book", {"action", "loaded", "entries", std::to_string(list_.size()), "elapsed",
                               StopWatch::format(sw.since_start())});
}

void AddressBook::save() const {
    if (empty()) return;

    bool expected{false};
    if (not is_saving_.compare_exchange_strong(expected, false)) return;

    std::scoped_lock lock{mutex_};
    StopWatch sw(/*auto_start=*/true);
    log::Info("Address Book", {"action", "saving", "entries", std::to_string(list_.size())}) << "...";

    try {
        auto& env_config{app_settings_.nodedata_env_config};
        env_config.path = (*app_settings_.data_directory)[DataDirectory::kNodesName].path().string();
        env_config.create = !std::filesystem::exists(db::get_datafile_path(env_config.path));
        env_config.exclusive = true;

        log::Info("Opening database", {"path", env_config.path});
        auto node_data_env{db::open_env(env_config)};
        db::RWTxn txn(node_data_env);
        db::tables::deploy_tables(*txn, db::tables::kNodeDataTables);
        txn.commit(/*renew=*/true);
        db::write_config_value(*txn, "seed", key_);
        txn->clear_map(db::tables::kServices.name);
        txn->clear_map(db::tables::kRandomOrder.name);
        txn->clear_map(db::tables::kBuckets.name);
        txn.commit(/*renew=*/true);
        db::Cursor cursor(txn, db::tables::kServices);

        ser::SDataStream data_stream(ser::Scope::kStorage, 0);
        Bytes key(sizeof(uint32_t), 0);
        auto& idx{list_index_.get<by_id>()};

        // Save all entries
        for (auto it{idx.begin()}; it not_eq idx.end(); ++it) {
            data_stream.clear();
            const auto result{it->list_it->serialize(data_stream)};
            if (result.has_error()) {
                log::Error("Address Book", {"action", "serialization", "error", result.error().message()});
                break;
            }

            endian::store_big_u32(key.data(), it->id);
            const auto k{db::to_slice(key)};
            const auto v{db::to_slice(data_stream.read().value())};
            cursor.upsert(k, v);
        }

        // Save the contents of randomly ordered ids
        uint32_t ordinal{0U};
        Bytes value(sizeof(uint32_t), 0);
        cursor.bind(txn, db::tables::kRandomOrder);
        for (const auto entry_id : randomly_ordered_ids_) {
            endian::store_big_u32(key.data(), ordinal++);
            endian::store_big_u32(value.data(), entry_id);
            const auto k{db::to_slice(key)};
            const auto v{db::to_slice(value)};
            cursor.upsert(k, v);
        }

        // Save the contents of New and Tried buckets
        cursor.bind(txn, db::tables::kBuckets);
        key.insert(key.begin(), 'N');
        for (const auto& [slot_address, entry_id] : new_buckets_) {
            endian::store_big_u32(key.data() + 1, slot_address);
            endian::store_big_u32(value.data(), entry_id);
            const auto k{db::to_slice(key)};
            const auto v{db::to_slice(value)};
            cursor.upsert(k, v);
        }
        key[0] = 'T';
        for (const auto& [slot_address, entry_id] : tried_buckets_) {
            endian::store_big_u32(key.data() + 1, slot_address);
            endian::store_big_u32(value.data(), entry_id);
            const auto k{db::to_slice(key)};
            const auto v{db::to_slice(value)};
            cursor.upsert(k, v);
        }

        cursor.close();
        txn.commit(/*renew=*/false);
        log::Info("Closing database", {"path", env_config.path});
        node_data_env.close();
    } catch (const std::exception& ex) {
        log::Error("Address Book", {"action", "saving", "error", ex.what()});
        is_saving_.exchange(false);
        return;
    }

    log::Info("Address Book", {"action", "saved", "entries", std::to_string(list_.size()), "elapsed",
                               StopWatch::format(sw.since_start())});
    is_saving_.exchange(false);
};

void AddressBook::on_service_timer_expired(con::Timer::duration& interval) {
    if (!is_running()) return;
    // TODO clean up outdated entries
    save();
}

void AddressBook::erase_new_entry(uint32_t entry_id) noexcept {
    if (entry_id == 0U) return;  // Cannot erase non-existent entry
    auto& idx{list_index_.get<by_id>()};
    auto it{idx.find(entry_id)};
    ASSERT(it not_eq idx.end());  // Must be found or else the data structures are inconsistent
    auto& entry{*it->list_it};
    ASSERT(entry.new_refs_.empty());        // Must not be referenced by any "new" bucket
    ASSERT(!entry.tried_ref_.has_value());  // Must not be in the tried bucket

    swap_randomly_ordered_ids(entry.random_pos_, static_cast<uint32_t>(randomly_ordered_ids_.size() - 1U));
    ASSERT(randomly_ordered_ids_.back() == entry_id);  // Must have become the last element
    randomly_ordered_ids_.pop_back();
    --new_entries_size_;
    std::ignore = list_.erase(it->list_it);
    std::ignore = idx.erase(it);
}

void AddressBook::make_entry_tried(uint32_t entry_id) noexcept {
    using namespace std::chrono_literals;

    NodeServiceInfo* service_info{nullptr};
    std::tie(service_info, std::ignore) = lookup_entry(entry_id);
    ASSERT(service_info != nullptr);
    if (service_info->tried_ref_.has_value()) return;  // Already in the tried bucket

    // Erase all references from the "new" buckets
    new_entries_size_ -= static_cast<uint32_t>(!service_info->new_refs_.empty());
    for (auto refs_iterator{service_info->new_refs_.begin()}; refs_iterator not_eq service_info->new_refs_.end();) {
        new_buckets_.erase(*refs_iterator);
        refs_iterator = service_info->new_refs_.erase(refs_iterator);
    }

    const auto tried_slot_address{get_tried_slot(*service_info)};
    const auto tried_slot_it{tried_buckets_.find(tried_slot_address.xy)};

    if (tried_slot_it not_eq tried_buckets_.end()) {
        // Evict existing item from the tried bucket
        auto [evict_service_info, evict_entry_id]{lookup_entry(tried_slot_it->second)};
        ASSERT(evict_service_info != nullptr);

        ASSERT(evict_service_info->tried_ref_.has_value() &&
               evict_service_info->tried_ref_.value() == tried_slot_address.xy);  // Must be in the tried bucket
        ASSERT(evict_service_info->new_refs_.empty());                            // Must not be referenced by any "new"
                                                                                  // bucket
        evict_service_info->tried_ref_.reset();
        tried_buckets_.erase(tried_slot_it);
        --tried_entries_size_;

        // Get coordinates for a bucket positioning in the "new" collection
        // and eventually put a reference to the evicted item
        const auto slot_address{get_new_slot(*evict_service_info, evict_service_info->origin_)};
        clear_new_slot(slot_address, true);  // Make room for the new entry if necessary
        ASSERT(evict_service_info->new_refs_.emplace(slot_address.xy).second);  // Must be inserted
        ASSERT(new_buckets_.emplace(slot_address.xy, evict_entry_id).second);   // Must be inserted
        ++new_entries_size_;
    }

    ASSERT(tried_buckets_.emplace(tried_slot_address.xy, entry_id).second);  // Must be inserted
    service_info->tried_ref_.emplace(tried_slot_address.xy);
    ++tried_entries_size_;
}

void AddressBook::clear_new_slot(const SlotAddress& slot_address, bool erase_unreferenced_entry) noexcept {
    ASSERT(slot_address.x < kNewBucketsCount and slot_address.y < kBucketSize);
    auto slot_it{new_buckets_.find(slot_address.xy)};
    if (slot_it == new_buckets_.end()) return;  // Empty slot already

    {
        auto [service_info, entry_id]{lookup_entry(slot_it->second)};
        ASSERT(entry_id != 0U);                                        // Must be found
        ASSERT(service_info->new_refs_.erase(slot_address.xy) == 1U);  // Must be erased
        if (!service_info->new_refs_.empty() or service_info->tried_ref_.has_value()) {
            erase_unreferenced_entry = false;  // Do not erase the entry
        }
    }

    // Remove the service from the address book entirely
    if (erase_unreferenced_entry) erase_new_entry(slot_it->second);
    new_buckets_.erase(slot_it);  // Effectively clear the slot
}

std::pair<NodeServiceInfo*, /*id*/ uint32_t> AddressBook::lookup_entry(const IPEndpoint& endpoint) const noexcept {
    auto& idx{list_index_.get<by_endpoint>()};
    const auto it{idx.find(endpoint)};
    if (it == idx.end()) {
        return {nullptr, 0U};
    }
    NodeServiceInfo* entry{&(*(it->list_it))};
    return {entry, it->id};
}

std::pair<NodeServiceInfo*, /*id*/ uint32_t> AddressBook::lookup_entry(const uint32_t entry_id) const noexcept {
    auto& idx{list_index_.get<by_id>()};
    const auto it{idx.find(entry_id)};
    if (it == idx.end()) {
        return {nullptr, 0U};
    }
    NodeServiceInfo* entry{&(*(it->list_it))};
    return {entry, it->id};
}

void AddressBook::swap_randomly_ordered_ids(uint32_t i, uint32_t j) noexcept {
    if (i == j) return;
    ASSERT(i < randomly_ordered_ids_.size() and j < randomly_ordered_ids_.size());  // Must be valid indices

    auto id_at_i{randomly_ordered_ids_[i]};
    auto id_at_j{randomly_ordered_ids_[j]};

    auto& idx{list_index_.get<by_id>()};
    auto it1{idx.find(id_at_i)};
    auto it2{idx.find(id_at_j)};
    ASSERT(it1 not_eq idx.end());  // Must be found
    ASSERT(it2 not_eq idx.end());  // Must be found

    // Swap the references
    it1->list_it->random_pos_ = j;
    it2->list_it->random_pos_ = i;

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

std::pair<NodeServiceInfo*, bool> AddressBook::insert_or_update_impl(NodeService& service, const IPAddress& source,
                                                                     std::chrono::seconds time_penalty) {
    using namespace std::chrono_literals;
    if (source == service.endpoint_.address_) time_penalty = 0s;  // Self advertisement
    std::pair<NodeServiceInfo*, bool> ret{nullptr, false};
    auto [entry, entry_id]{lookup_entry(service.endpoint_)};
    if (entry != nullptr) {
        update_entry(*entry, entry_id, service, source, time_penalty);
        ret.first = entry;
        ret.second = false;
    } else {
        // Insert new item
        std::tie(entry, entry_id) = insert_entry(service, source, time_penalty);
        entry->service_.time_ = std::max(NodeSeconds{NodeService::kTimeInit}, service.time_ - time_penalty);
        ret.first = entry;
        ret.second = true;
    }
    return ret;
}

std::pair<NodeServiceInfo*, uint32_t> AddressBook::insert_entry(const NodeService& service, const IPAddress& source,
                                                                std::chrono::seconds time_penalty) noexcept {
    uint32_t new_id{last_used_id_.fetch_add(1U)};
    NodeServiceInfo service_info{service, source};
    service_info.service_.time_ -= time_penalty;

    // Get coordinates of the bucket and position in the new bucket
    // and eventually put a reference to the entry in the new bucket
    const auto slot_address{get_new_slot(service_info, source)};
    clear_new_slot(slot_address, true);                              // Make room for the new entry if necessary
    ASSERT(service_info.new_refs_.emplace(slot_address.xy).second);  // Must be inserted
    ASSERT(new_buckets_.emplace(slot_address.xy, new_id).second);    // Must be inserted

    service_info.random_pos_ = static_cast<uint32_t>(randomly_ordered_ids_.size());
    randomly_ordered_ids_.push_back(new_id);

    auto new_it{list_.emplace(list_.end(), std::move(service_info))};
    list_index_.insert({new_id, service.endpoint_, new_it});
    ++new_entries_size_;
    return {new_it.operator->(), new_id};
}

void AddressBook::update_entry(NodeServiceInfo& entry, const uint32_t entry_id, const NodeService& service,
                               const IPAddress& source, std::chrono::seconds time_penalty) noexcept {
    // Update time seen
    using namespace std::chrono_literals;
    const bool currently_online{NodeClock::now() - service.time_ < 24h};
    const auto update_interval{currently_online ? 1h : 24h};
    if (entry.service_.time_ < (service.time_ - update_interval - time_penalty)) {
        entry.service_.time_ = std::max(NodeSeconds{NodeService::kTimeInit}, service.time_ - time_penalty);
    }
    entry.service_.services_ |= service.services_;

    // Sanity check : entry must be either in the new bucket or in the tried bucket but not both
    ASSERT(!entry.new_refs_.empty() xor entry.tried_ref_.has_value());

    // Do not update when:
    // 1. The provided service has a seen date older than the one in the address book
    // 2. The entry is already in the tried bucket
    // 3. The entry has already been referenced kMaxNewBucketReferences times
    if (entry.service_.time_ < service.time_ or entry.tried_ref_.has_value() or
        entry.new_refs_.size() == kMaxNewBucketReferences) {
        return;
    }

    // Stochastic test : previous new_references_count_ == N: 2^N times harder to increase it
    uint32_t factor{1U << entry.new_refs_.size()};
    if (randomize<uint32_t>(0U, factor - 1) not_eq 0) {
        return;
    }

    // Get coordinates of the bucket and position in the new bucket
    // and eventually put a reference to the entry in the new bucket
    const auto slot_address{get_new_slot(entry, source)};
    if (entry.new_refs_.contains(slot_address.xy)) {
        ASSERT(new_buckets_.contains(slot_address.xy));  // Must be found or else the data structures are inconsistent)
        return;
    }

    auto slot_it{new_buckets_.find(slot_address.xy)};
    if (slot_it != new_buckets_.end()) {
        ASSERT(slot_it->second != entry_id);  // Must not contain a reference to this entry otherwise the data
                                              // structures are inconsistent
        clear_new_slot(slot_address, true);   // Make room for the new entry
    }
    ASSERT(entry.new_refs_.emplace(slot_address.xy).second);         // Must be inserted
    ASSERT(new_buckets_.emplace(slot_address.xy, entry_id).second);  // Must be inserted
    if (entry.new_refs_.size() == 1U) ++new_entries_size_;
}
}  // namespace znode::net
