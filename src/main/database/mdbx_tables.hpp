/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>

#include <zen/node/common/version.hpp>
#include <zen/node/database/mdbx.hpp>

namespace zen::db::tables {

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

//! \brief Ensures all tables are properly deployed in database
//! \remarks Should a table already exist it's flags are not checked.
//! A change in table's flags MUST reflect in db schema version check hence handled by proper migrations
template <size_t N>
void deploy_tables(mdbx::txn& txn, const std::array<db::MapConfig, N> tables) {
    if (txn.is_readonly()) [[unlikely]]
        throw std::invalid_argument("Can't deploy tables on RO transaction");

    for (const auto& table : tables) {
        if (!has_map(txn, table.name)) [[unlikely]]
            std::ignore = txn.create_map(table.name, table.key_mode, table.value_mode);
    }
}

}  // namespace zen::db::tables
