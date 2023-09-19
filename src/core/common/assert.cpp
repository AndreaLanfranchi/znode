/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "assert.hpp"

#include <iostream>

namespace zenpp {
void abort_due_to_assertion_failure(std::string_view message, const std::source_location& location) {
    std::cerr << "\n!! Assertion failed !!\n"
              << "   Expression: " << message << "\n"
              << "   Function  : " << location.function_name() << "\n"
              << "   Source    : " << location.file_name() << ", line " << location.line() << "\n\n"
              << "** Please report this to developers **. Aborting ...\n"
              << std::endl;
    std::abort();
}
}  // namespace zenpp