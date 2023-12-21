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

#include <catch2/catch.hpp>

#include <infra/database/access_layer.hpp>
#include <infra/filesystem/directories.hpp>

namespace znode::db {

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
}  // namespace znode::db
