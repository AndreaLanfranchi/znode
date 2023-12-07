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

#pragma once
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <core/common/random.hpp>
#include <core/types/hash.hpp>

#include <infra/network/addresses.hpp>

namespace znode::net {

class AddressBook {
  public:
    static constexpr uint16_t kBucketSize{64};
    static constexpr uint16_t kNewBucketsCount{1024};
    static constexpr uint16_t kTriedBucketsCount{256};
    static constexpr uint16_t kMaxNewBucketReferences{8};
    static constexpr uint16_t kNewBucketsPerSourceGroup{64};
    static constexpr uint16_t kTriedBucketsPerGroup{8};
    static constexpr uint16_t kIPv4SubnetGroupsPrefix{16};
    static constexpr uint16_t kIPv6SubnetGroupsPrefix{64};

    AddressBook() = default;
    ~AddressBook() = default;

#if defined(_MSC_VER)
    // Silence C4201 under MSVC
    // https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-4-c4201
#pragma warning(push)
#pragma warning(disable : 4201)
#endif

    //! \brief A struct representing the coordinates of a slot in collection of buckets
    struct SlotAddress {
        SlotAddress() noexcept : x{0}, y{0} {}
        explicit SlotAddress(uint16_t pos, uint16_t num) noexcept : x{pos}, y{num} {}
        explicit SlotAddress(uint32_t pos_num) noexcept : xy{pos_num} {}
        union alignas(uint32_t) {
            struct alignas(uint16_t) {
                uint16_t x;
                uint16_t y;
            };
            uint32_t xy;
        };
        std::strong_ordering operator<=>(const SlotAddress& other) const { return xy <=> other.xy; }
        bool operator==(const SlotAddress& other) const { return xy == other.xy; }
    };

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    // See struct for coordinates in NodeServiceInfo
    static_assert(kBucketSize <= std::numeric_limits<uint16_t>::max());
    static_assert(kNewBucketsCount <= std::numeric_limits<uint16_t>::max());
    static_assert(kTriedBucketsCount <= std::numeric_limits<uint16_t>::max());

    //! \brief Returns the overall size of the address book
    [[nodiscard]] size_t size() const;

    //! \brief Returns whether the address book is empty
    [[nodiscard]] bool empty() const;

    //! \brief Inserts or updates an item in the address book
    //! \details If the item is already in the address book, it is updated with the new information
    //! \returns true if the item was inserted, false if it was updated
    //! \throws std::invalid_argument it the network address is not routable
    [[nodiscard]] bool add_new(NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty);

    //! \brief Inserts or updates a vector of item in the address book
    //! \details If the item is already in the address book, it is updated with the new information
    //! \returns true if any item was inserted, false otherwise
    [[nodiscard]] bool add_new(std::vector<NodeService>& services, const IPAddress& source,
                               std::chrono::seconds time_penalty);

    //! \brief Marks an item as good (reachable and successfully connected to)
    [[nodiscard]] bool set_good(const IPEndpoint& remote, NodeSeconds time = Now<NodeSeconds>()) noexcept;

    //! \brief Returns whether a NodeService is contained in the address book
    [[nodiscard]] bool contains(const NodeService& service) const noexcept;

    //! \brief Returns whether an endpoint is contained in the address book
    [[nodiscard]] bool contains(const IPEndpoint& endpoint) const noexcept;

    //! \brief Returns whether an id is contained in the address book
    [[nodiscard]] bool contains(uint32_t id) const noexcept;

    //! \brief Selects a random entry from the address book for a new dial-out connection
    //! \param new_only Whether to only consider entries in the "new" buckets
    //! \param type The type of address to select (IPv4 or IPv6). If not provided, any type is considered
    //! \returns A pair containing the endpoint and the time it was last tried
    [[nodiscard]] std::pair<std::optional<IPEndpoint>, NodeSeconds> select_random(
        bool new_only, std::optional<IPAddressType> type = std::nullopt) const noexcept;

  private:
    mutable std::shared_mutex mutex_;                                      // Thread safety
    const Bytes key_{get_random_bytes(2 * sizeof(uint64_t))};              // Secret key to randomize the address book
    std::atomic<uint32_t> last_used_id_{1};                                // Last used id (0 means "non-existent")
    std::atomic<uint32_t> new_entries_size_{0};                            // Number of items in "new" buckets
    std::atomic<uint32_t> tried_entries_size_{0};                          // Number of items in "tried" buckets
    mutable std::vector<uint32_t> randomly_ordered_ids_;                   // Randomly ordered ids
    std::unordered_map<uint32_t, NodeServiceInfo> map_id_to_serviceinfo_;  // Index id -> ServiceInfo
    std::unordered_map<IPEndpoint, uint32_t, IPEndpointHasher> map_endpoint_to_id_;  // Index endpoint -> id

    /* Buckets */
    using bucket_t = std::array<uint32_t, kBucketSize>;
    std::array<bucket_t, kNewBucketsCount> new_buckets_{};      // New buckets (all zeroed)
    std::array<bucket_t, kTriedBucketsCount> tried_buckets_{};  // Tried buckets (all zeroed)

    /*
     * Note ! Private methods, if called from public methods, assume that the caller has already acquired a lock
     */

    //! \brief Inserts or updates an item in the address book
    //! \details If the item is already in the address book, it is updated with the new information
    //! \returns true if the item was inserted, false if it was updated
    //! \throws std::invalid_argument it the network address is not routable
    [[nodiscard]] bool add_new_impl(NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty);

    //! \brief Create a "new" entry and add it to the internal data structures
    //! \returns A pair containing a pointer to the newly created entry and its newly generated id
    std::pair<NodeServiceInfo*, /*id*/ uint32_t> emplace_new_entry(const NodeService& service,
                                                                   const IPAddress& source) noexcept;

    //! \brief Erases an entry from the address book entirely when is only in the "new" bucket
    void erase_new_entry(uint32_t id) noexcept;

    //! \brief Removes an entry from any "new" bucket and adds it to a "tried" bucket
    //! \remarks Shouldn't be room in the "tried" bucket, the previous entry is erased and
    //! pushed back to the "new" bucket
    void make_entry_tried(uint32_t entry_id) noexcept;

    //! \brief Removes a reference of NodeServiceInfo from a "new" bucket's slot and optionally erases
    //! the entry if it was the last reference
    void clear_new_slot(const SlotAddress& slot_address, bool erase_entry = true) noexcept;

    //! \brief Queries internal data structures to find whether the provided service exists
    //! \returns A pair containing a pointer to the entry and its id if it exists, nullptr and 0 otherwise
    std::pair<NodeServiceInfo*, /*id*/ uint32_t> find_entry(const NodeService& service);

    //! \brief Exchange two ids in the randomly ordered ids vector
    void swap_randomly_ordered_ids(uint32_t i, uint32_t j) noexcept;

    //! \brief Computes the coordinates for placement in a "new" bucket's slot
    SlotAddress get_new_slot(const NodeServiceInfo& service, const IPAddress& source) const noexcept;

    //! \brief Computes the coordinates for placement in a "tried" bucket's slot
    SlotAddress get_tried_slot(const NodeServiceInfo& service) const noexcept;

    //! \brief Computes the group an address belongs to
    //! \details The computation is based on finding the base address for an IP subnet
    //! (i.e. kIPv4SubnetGroupsPrefix and kIPv6SubnetGroupsPrefix respectively)
    static Bytes compute_group(const IPAddress& address) noexcept;

    //! \brief Pulls the entry id registered at provided slot address
    //! \param slot The slot address
    //! \param tried Whether the slot is in a "tried" bucket. Otherwise is sought in a "new" bucket
    uint32_t get_entry_id(SlotAddress slot, bool tried) const noexcept;
};
}  // namespace znode::net
