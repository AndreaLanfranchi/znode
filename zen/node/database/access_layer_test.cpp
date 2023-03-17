/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/database/access_layer.hpp>

namespace zen::db {

TEST_CASE("Schema version", "[database]") {
    const TempDirectory tmp_dir{};
    EnvConfig db_config{tmp_dir.path().string(), /*create=*/true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    RWTxn txn(env);

    auto version{read_schema_version(*txn)};
    CHECK_FALSE(version);  // Not found

    version.emplace(Version{1, 0, 0});
    write_schema_version(*txn, *version);
    CHECK(read_schema_version(*txn)->to_string() == "1.0.0");

    version->Major = 0;
    version->Minor = 1;
    CHECK_THROWS(write_schema_version(*txn, *version));
}

TEST_CASE("Deploy Tables", "[database]") {
    const TempDirectory tmp_dir{};
    EnvConfig db_config{tmp_dir.path().string(), /*create=*/true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    RWTxn txn(env);

    for (const auto& table : tables::kChainDataTables) {
        CHECK_FALSE(has_map(*txn, table.name));
    }

    tables::deploy_tables(*txn, tables::kChainDataTables);
    txn.commit();
    for (const auto& table : tables::kChainDataTables) {
        CHECK(has_map(*txn, table.name));
    }
}
}  // namespace zen::db
