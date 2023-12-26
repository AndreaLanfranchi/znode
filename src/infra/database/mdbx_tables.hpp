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

#include <array>
#include <ranges>

#include <infra/common/version.hpp>
#include <infra/database/mdbx.hpp>

namespace znode::db::tables {

//! \brief Specifies the schema version we're compatible with
//! \remarks This is also used in checking whether the database needs an ugrade migration
inline constexpr Version kRequiredSchemaVersion{1, 0, 0};  // We're compatible with this

/* List of database canonical tables and their descriptions */

//! \details Stores relevant configuration values for db and node
//! \struct
//! \verbatim
//!   key   : value of configuration key
//!   value : value of configuration value
//! \endverbatim
inline constexpr db::MapConfig kConfig{"Config"};
inline constexpr std::string_view kConfigChainKey{"chain"};

//! \details Stores Block headers information
//! \struct
//! \verbatim
//!   key   : TODO
//!   value : TODO
//! \endverbatim
inline constexpr db::MapConfig kHeaders{"Headers"};

inline constexpr const char* kDbSchemaVersionKey{"DbSchemaVersion"};

//! \details Stores reached progress for each stage
//! \struct
//! \verbatim
//!   key   : stage name
//!   value : block_num_u32 (BE)
//! \endverbatim
inline constexpr db::MapConfig kSyncStageProgress{"Stages"};

//! \brief List of all Chaindata database tables
inline constexpr std::array<db::MapConfig, 3> kChainDataTables{kConfig, kHeaders, kSyncStageProgress};

//! \details Stores list of known peer addresses and related info
//! \struct
//! \verbatim
//!   key   : uint32_t id (BE)
//!   value : NodeServiceInfo (serialized)
//! \endverbatim
inline constexpr db::MapConfig kServices{"Services"};

//! \details Stores the contents of AddressBook::randomly_ordered_ids_
//! \struct
//! \verbatim
//!   key   : uint32_t ordinal position (BE)
//!   value : uint32_t entry_id (BE)
//! \endverbatim
inline constexpr db::MapConfig kRandomOrder{"RandomOrder"};

//! \details Stores the contents of AddressBook's buckets
//! \struct
//! \verbatim
//!   key   : 'N' (New) / 'T' (Tried) + uint32_t bucket address (BE)
//!   value : uint32_t entry_id (BE)
//! \endverbatim
inline constexpr db::MapConfig kBuckets{"Buckets"};


//! \brief List of all Nodes database tables
inline constexpr std::array<db::MapConfig, 4> kNodeDataTables{kConfig, kServices, kRandomOrder, kBuckets};

//! \brief Ensures all tables are properly deployed in database
//! \remarks Should a table already exist it's flags are not checked.
//! A change in table's flags MUST reflect in db schema version check hence handled by proper migrations
template <size_t N>
void deploy_tables(mdbx::txn& txn, const std::array<db::MapConfig, N> tables) {
    if (txn.is_readonly()) [[unlikely]]
        throw std::invalid_argument("Can't deploy tables on RO transaction");

    std::ranges::for_each(tables, [&txn](const auto& table) {
        if (has_map(txn, table.name)) return;
        std::ignore = txn.create_map(table.name, table.key_mode, table.value_mode);
    });
}
}  // namespace znode::db::tables
