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

#include "assert.hpp"

#include <iostream>

namespace znode {
void abort_due_to_assertion_failure(std::string_view message, const std::source_location& location) {
    std::cerr << "\n!! Assertion failed !!\n"
              << "   Expression: " << message << "\n"
              << "   Function  : " << location.function_name() << "\n"
              << "   Source    : " << location.file_name() << ", line " << location.line() << "\n\n"
              << "** Please report this to developers **. Aborting ...\n"
              << std::endl;
    std::abort();
}
}  // namespace znode