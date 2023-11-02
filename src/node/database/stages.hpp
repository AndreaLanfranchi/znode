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

#include <node/database/mdbx_tables.hpp>

namespace znode::db::stages {

//! \brief Headers are downloaded, their Proof-Of-Work validity and chaining is verified
inline constexpr const char* kHeadersKey{"Headers"};

//! \brief Block bodies are downloaded and partially verified
inline constexpr const char* kBlockBodiesKey{"Bodies"};

//! \brief Executing each block
inline constexpr const char* kExecutionKey{"Execution"};

//! \brief Nominal stage after all other stages
inline constexpr const char* kFinishKey{"Finish"};

//! \brief Not an actual stage rather placeholder for global unwind point
inline constexpr const char* kUnwindKey{"Unwind"};

//! \brief List of all known stages
inline constexpr const char* kAllStages[]{
    kHeadersKey, kBlockBodiesKey, kExecutionKey, kFinishKey, kUnwindKey,
};

//! \brief Stages won't log their "start" if segment is below this threshold
inline constexpr size_t kSmallBlockSegmentWidth{16};

//! \brief Some stages will use this threshold to determine if worth regen vs incremental
inline constexpr size_t kLargeBlockSegmentWorthRegen{100'000};

//! \brief Reads from db the progress (block height) of the provided stage name
//! \param [in] txn : a reference to a ro/rw db transaction
//! \param [in] stage_name : the name of the requested stage (must be known see kAllStages[])
//! \return The actual chain height (BlockNum) the stage has reached
BlockNum read_stage_progress(mdbx::txn& txn, const char* stage_name);

//! \brief Reads from db the prune progress (block height) of the provided stage name
//! \param [in] txn : a reference to a ro/rw db transaction
//! \param [in] stage_name : the name of the requested stage (must be known see kAllStages[])
//! \return The actual chain height (BlockNum) the stage has pruned its data up to
//! \remarks A pruned height X means the prune stage function has run up to this block
BlockNum read_stage_prune_progress(mdbx::txn& txn, const char* stage_name);

//! \brief Writes into db the progress (block height) for the provided stage name
//! \param [in] txn : a reference to a rw db transaction
//! \param [in] stage_name : the name of the involved stage (must be known see kAllStages[])
//! \param [in] block_num : the actual chain height (BlockNum) the stage must record
void write_stage_progress(mdbx::txn& txn, const char* stage_name, BlockNum block_num);

//! \brief Writes into db the prune progress (block height) for the provided stage name
//! \param [in] txn : a reference to a rw db transaction
//! \param [in] stage_name : the name of the involved stage (must be known see kAllStages[])
//! \param [in] block_num : the actual chain height (BlockNum) the stage must record
//! \remarks A pruned height X means the prune stage function has run up to this block
void write_stage_prune_progress(mdbx::txn& txn, const char* stage_name, BlockNum block_num);

//! \brief Whether the provided stage name is known
//! \param [in] stage_name : The name of the stage to check for
//! \return Whether it exists in kAllStages[]
bool is_known_stage(const char* stage_name);

}  // namespace znode::db::stages