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
#include <shared_mutex>

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

    //! \brief Returns whether an endpoint is contained in the address book
    [[nodiscard]] bool contains(const IPEndpoint& endpoint) const;

  private:
    mutable std::shared_mutex mutex_;  // Thread safety
    h256 key_;                         // Secret key to randomize the address book
};
}  // namespace znode::net
