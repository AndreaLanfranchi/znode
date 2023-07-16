/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>

#include <core/common/assert.hpp>

namespace zen {
void abort_due_to_assertion_failure(std::string_view message, const char* function, const char* file, long line) {
    std::cerr << "\n!! Assertion failed !!\n"
              << "   Expression: " << message << "\n"
              << "   Function  : " << function << "\n"
              << "   Source    : " << file << ", line " << line << "\n\n"
              << "** Please report this to developers **. Aborting ...\n"
              << std::endl;
    std::abort();
}
}  // namespace zen