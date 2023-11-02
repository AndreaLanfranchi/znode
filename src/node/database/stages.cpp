/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "stages.hpp"

#include <absl/strings/str_cat.h>

#include <core/common/endian.hpp>

namespace znode::db::stages {

namespace {

    BlockNum get_stage_data(mdbx::txn& txn, const char* stage_name, const db::MapConfig& domain,
                            const char* key_prefix = nullptr) {
        if (not is_known_stage(stage_name)) {
            throw std::invalid_argument("Unknown stage name " + std::string(stage_name));
        }

        try {
            db::Cursor src(txn, domain);
            std::string item_key{stage_name};
            if (key_prefix not_eq nullptr) {
                item_key.insert(0, std::string(key_prefix));
            }
            auto data{src.find(mdbx::slice(item_key.c_str()), /*throw_notfound*/ false)};
            if (not data) return 0;
            if (data.value.size() not_eq sizeof(BlockNum)) [[unlikely]] {
                throw std::length_error("Expected 4 bytes of data got " + std::to_string(data.value.size()));
            }
            return endian::load_big_u32(static_cast<uint8_t*>(data.value.data()));
        } catch (const mdbx::exception& ex) {
            const std::string what{absl::StrCat("Error in ", __func__, " ", ex.what())};
            throw std::runtime_error(what);
        }
    }

    void set_stage_data(mdbx::txn& txn, const char* stage_name, BlockNum block_num, const db::MapConfig& domain,
                        const char* key_prefix = nullptr) {
        if (not is_known_stage(stage_name)) {
            throw std::invalid_argument("Unknown stage name");
        }

        try {
            std::string item_key{stage_name};
            if (key_prefix not_eq nullptr) {
                item_key.insert(0, std::string(key_prefix));
            }
            Bytes stage_progress(sizeof(block_num), 0);
            endian::store_big_u32(stage_progress.data(), block_num);
            db::Cursor target(txn, domain);
            const mdbx::slice key(item_key.c_str());
            const mdbx::slice value{db::to_slice(stage_progress)};
            target.upsert(key, value);
        } catch (const mdbx::exception& ex) {
            const std::string what{absl::StrCat("Error in ", __func__, " ", ex.what())};
            throw std::runtime_error(what);
        }
    }

}  // namespace

BlockNum read_stage_progress(mdbx::txn& txn, const char* stage_name) {
    return get_stage_data(txn, stage_name, db::tables::kSyncStageProgress);
}

BlockNum read_stage_prune_progress(mdbx::txn& txn, const char* stage_name) {
    return get_stage_data(txn, stage_name, db::tables::kSyncStageProgress, "prune_");
}

void write_stage_progress(mdbx::txn& txn, const char* stage_name, BlockNum block_num) {
    set_stage_data(txn, stage_name, block_num, db::tables::kSyncStageProgress);
}

void write_stage_prune_progress(mdbx::txn& txn, const char* stage_name, BlockNum block_num) {
    set_stage_data(txn, stage_name, block_num, db::tables::kSyncStageProgress, "prune_");
}

bool is_known_stage(const char* name) {
    if (strlen(name) not_eq 0U) {
        for (const auto* stage : kAllStages) {
            if (strcmp(stage, name) == 0) {
                return true;
            }
        }
    }
    return false;
}
}  // namespace znode::db::stages
