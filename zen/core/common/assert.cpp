/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>

#include <zen/core/common/assert.hpp>

namespace zen {
void abort_due_to_assertion_failure(std::string_view message, const char* file, long line) {
    std::cerr << "Assert failed: " << message << "\n"
              << "Source: " << file << ", line " << line << std::endl;
    std::abort();
}
}  // namespace zen