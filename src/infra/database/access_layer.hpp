/*
   Copyright 2022 The Silkworm Authors
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

#include <optional>
#include <utility>

#include <core/chain/config.hpp>
#include <core/common/cast.hpp>
#include <core/common/endian.hpp>

#include <infra/database/mdbx_tables.hpp>

namespace znode::db {

class Exception : public std::exception {
  public:
    explicit Exception(std::string what) : what_{std::move(what)} {}

    [[nodiscard]] const char* what() const noexcept override { return what_.c_str(); }

  private:
    std::string what_;
};

//! \brief Upserts a key/value pair into Config table
void write_config_value(mdbx::txn& txn, std::string_view key, const ByteView& value);

//! \brief Pulls a value from Config table
std::optional<Bytes> read_config_value(mdbx::txn& txn, std::string_view key);

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