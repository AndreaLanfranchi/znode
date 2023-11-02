/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>
#include <utility>

#include <core/chain/config.hpp>
#include <core/common/cast.hpp>
#include <core/common/endian.hpp>

#include <node/database/mdbx_tables.hpp>

namespace znode::db {

class Exception : public std::exception {
  public:
    explicit Exception(std::string what) : what_{std::move(what)} {}

    [[nodiscard]] const char* what() const noexcept override { return what_.c_str(); }

  private:
    std::string what_;
};

//! \brief Pulls database schema version from Config table
std::optional<Version> read_schema_version(mdbx::txn& txn);

//! \brief Upserts database schema version into Config table
//! \remarks Should new version be LT previous version an exception is thrown
void write_schema_version(mdbx::txn& txn, const Version& version);

//! \brief Pulls chain config from Config table (if any)
std::optional<ChainConfig> read_chain_config(mdbx::txn& txn);

//! \brief Upserts chain config into Config table
void write_chain_config(mdbx::txn& txn, const ChainConfig& config);

}  // namespace znode::db