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
#include <vector>

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <core/types/hash.hpp>

#include <infra/common/random.hpp>
#include <infra/network/addresses.hpp>

namespace znode::net {

class AddressBook {
  public:
    AddressBook() : key_(get_random_bytes(32)) {}
    ~AddressBook() = default;

    //! \brief Returns the overall size of the address book
    [[nodiscard]] size_t size() const;

    //! \brief Returns whether the address book is empty
    [[nodiscard]] bool empty() const;

    //! \brief Inserts or updates an item in the address book
    //! \details If the item is already in the address book, it is updated with the new information
    //! \returns true if the item was inserted, false if it was updated
    //! \throws std::invalid_argument it the network address is not routable
    [[nodiscard]] bool upsert(const NodeService& service, const IPAddress& source, std::chrono::seconds time_penalty);

    //! \brief Returns whether a NodeService is contained in the address book
    [[nodiscard]] bool contains(const NodeService& service) const noexcept;

    //! \brief Returns whether an endpoint is contained in the address book
    [[nodiscard]] bool contains(const IPEndpoint& endpoint) const noexcept;

    //! \brief Returns whether an IP address is contained in the address book
    [[nodiscard]] bool contains(const IPAddress& address) const noexcept;

    //! \brief Returns whether an id is contained in the address book
    [[nodiscard]] bool contains(uint32_t id) const noexcept;

  private:
    mutable std::shared_mutex mutex_;      // Thread safety
    h256 key_;                             // Secret key to randomize the address book
    std::atomic<uint32_t> last_id_{1};     // Last used id (0 is reserved for "non-existent")
    std::atomic<uint32_t> new_size_{0};    // Number of items in "new" buckets
    std::atomic<uint32_t> tried_size_{0};  // Number of items in "tried" buckets

    //! \brief An entry in the address book
    struct book_entry {
        IPAddress address_;
        uint32_t id_;
        NodeServiceInfo item_;
    };
    struct by_address {};
    struct by_id {};

    //! \brief Holds all the ids in the address book and can be shuffled
    std::vector<uint32_t> randomly_ordered_ids_;

    //! \brief The multi-index container that holds the address book
    //! \details The container is indexed by IP address and id which must be unique
    boost::multi_index_container<
        book_entry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<by_address>,
                boost::multi_index::member<book_entry, IPAddress, &book_entry::address_>>,
            boost::multi_index::ordered_unique<boost::multi_index::tag<by_id>,
                                               boost::multi_index::member<book_entry, uint32_t, &book_entry::id_>>>>
        book_;
};
}  // namespace znode::net
