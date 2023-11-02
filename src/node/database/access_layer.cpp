/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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