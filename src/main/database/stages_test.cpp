/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/database/stages.hpp>

namespace zen::db {

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
}  // namespace zen::db
