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

#include "access_layer.hpp"

#include <core/common/assert.hpp>

namespace znode::db {

std::optional<Version> read_schema_version(mdbx::txn& txn) {
    Cursor config(txn, db::tables::kConfig);
    if (not config.seek(to_slice(tables::kDbSchemaVersionKey))) {
        return std::nullopt;
    }
    auto data{config.current()};
    ASSERT_POST(data.value.length() == sizeof(Version) and "Invalid serialized schema");
    const auto* data_ptr = static_cast<uint8_t*>(data.value.data());
    Version ret{};
    ret.Major = endian::load_big_u32(&data_ptr[0]);
    ret.Minor = endian::load_big_u32(&data_ptr[4]);
    ret.Patch = endian::load_big_u32(&data_ptr[8]);
    return ret;
}
void write_schema_version(mdbx::txn& txn, const Version& version) {
    if (txn.is_readonly()) return;
    const auto prev_version{read_schema_version(txn)};
    if (version == prev_version) return;  // no need to update
    if (version < prev_version) [[unlikely]] {
        throw std::invalid_argument("New version LT previous version");
    }
    Bytes value(sizeof(version), '\0');
    endian::store_big_u32(value.data(), version.Major);
    endian::store_big_u32(&value[4], version.Minor);
    endian::store_big_u32(&value[8], version.Patch);

    Cursor config(txn, db::tables::kConfig);
    config.upsert(to_slice(tables::kDbSchemaVersionKey), to_slice(value));
}

std::optional<ChainConfig> read_chain_config(mdbx::txn& txn) {
    Cursor src(txn, db::tables::kConfig);
    const auto data{src.find(to_slice(tables::kConfigChainKey), /*throw_notfound*/ false)};
    if (not data) return std::nullopt;

    // https://github.com/nlohmann/json/issues/2204
    const auto json = nlohmann::json::parse(data.value.as_string(), nullptr, false);
    return ChainConfig::from_json(json);
}

void write_chain_config(mdbx::txn& txn, const ChainConfig& config) {
    if (txn.is_readonly()) return;
    const auto json{config.to_json()};
    const auto json_str{json.dump()};
    Cursor config_cursor(txn, db::tables::kConfig);
    config_cursor.upsert(to_slice(tables::kConfigChainKey), to_slice(json_str));
}
}  // namespace znode::db