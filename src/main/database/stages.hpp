/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/node/database/mdbx_tables.hpp>

namespace zen::db::stages {

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

}  // namespace zen::db::stages