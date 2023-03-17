/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "access_layer.hpp"

#include <zen/core/common/assert.hpp>

namespace zen::db {

std::optional<Version> read_schema_version(mdbx::txn& txn) {
    Cursor config(txn, db::tables::kConfig);
    if (!config.seek(to_slice(tables::kDbSchemaVersionKey))) {
        return std::nullopt;
    }
    auto data{config.current()};
    ZEN_ASSERT(data.value.length() == sizeof(Version));
    const auto data_ptr = static_cast<uint8_t*>(data.value.data());
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
    endian::store_big_u32(&value[0], version.Major);
    endian::store_big_u32(&value[4], version.Minor);
    endian::store_big_u32(&value[8], version.Patch);

    Cursor config(txn, db::tables::kConfig);
    config.upsert(to_slice(tables::kDbSchemaVersionKey), to_slice(value));
}

}  // namespace zen::db