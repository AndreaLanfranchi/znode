/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/node/common/log.hpp>

namespace zen::log {

//! \brief Utility class using RAII to change the log verbosity level (necessary to make tests work in shuffled order)
class SetLogVerbosityGuard {
  public:
    explicit SetLogVerbosityGuard(log::Level new_level) { set_verbosity(new_level); }
    ~SetLogVerbosityGuard() { log::set_verbosity(current_level_); }

  private:
    log::Level current_level_{log::get_verbosity()};
};

//! \brief Factory function creating one null output stream (all characters are discarded)
inline std::ostream& null_stream() {
    static struct null_buf : public std::streambuf {
        int overflow(int c) override { return c; }
    } null_buf;
    static struct null_strm : public std::ostream {
        null_strm() : std::ostream(&null_buf) {}
    } null_strm;
    return null_strm;
}
}  // namespace zen::log
