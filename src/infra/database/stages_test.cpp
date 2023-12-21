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

#include <infra/database/stages.hpp>
#include <infra/filesystem/directories.hpp>

namespace znode::db {

TEST_CASE("Stages Progresses", "[database]") {
    const TempDirectory tmp_dir{};
    EnvConfig db_config{tmp_dir.path().string(), /*create=*/true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    RWTxn txn{RWTxn(env)};

    Cursor source(txn, tables::kSyncStageProgress);
    for (const auto stage_name : stages::kAllStages) {
        CHECK(stages::read_stage_progress(*txn, stage_name) == 0);
    }

    const BlockNum progress{1'200'000};
    stages::write_stage_progress(*txn, stages::kBlockBodiesKey, progress);
    CHECK(stages::read_stage_progress(*txn, stages::kBlockBodiesKey) == progress);

    txn.commit();
    source.bind(*txn, tables::kSyncStageProgress);
    CHECK(stages::read_stage_progress(*txn, stages::kBlockBodiesKey) == progress);
    CHECK(source.size() == 1);

    CHECK_THROWS((void)stages::read_stage_progress(*txn, "UnknownStageName"));
}
}  // namespace znode::db
