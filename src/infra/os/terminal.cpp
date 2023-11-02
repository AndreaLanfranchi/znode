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

#if defined(_WIN32)
#include <windows.h>
#if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#include "terminal.hpp"

#include <iostream>
#include <regex>

namespace znode {

void init_terminal() {
#if defined(_WIN32)
    // Change code page to UTF-8 so log characters are displayed correctly in console
    // and also support virtual terminal processing for coloring output
    SetConsoleOutputCP(CP_UTF8);
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output_handle != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(output_handle, &mode) not_eq 0) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(output_handle, mode);
        }
    }
#endif
}
bool ask_user_confirmation(const std::string message) {
    static const std::regex pattern{"^([yY])?([nN])?$"};
    std::smatch matches;
    std::string answer;
    do {
        std::cout << "\n" << message << " [y/N] ";
        std::cin >> answer;
        std::cin.clear();
        if (std::regex_search(answer, matches, pattern, std::regex_constants::match_default)) {
            break;
        }
        std::cout << "Hmmm... maybe you didn't read carefully. I repeat:" << std::endl;
    } while (true);

    return matches[1].length() > 0;
}
}  // namespace znode
