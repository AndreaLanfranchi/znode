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
#include <atomic>
#include <functional>
#include <string>

namespace znode::os {

//! \brief Specific exception for
class signal_exception : public std::exception {
  public:
    explicit signal_exception(int code);
    [[nodiscard]] const char* what() const noexcept final;
    [[nodiscard]] int sig_code() const noexcept { return sig_code_; }

  private:
    int sig_code_;
    std::string message_;
};

//! \brief Handler for OS Signals traps
class Signals {
  public:
    static void init(std::function<void(int)> custom_handler = {}, bool silent = false);  // Enable the hooks
    static void handle(int sig_code);                                                     // Handles incoming signal
    [[nodiscard]] static bool signalled() { return signalled_; }  // Whether a signal has been intercepted
    static void reset() noexcept;                                 // Reset to un-signalled (see tests coverage)
    static void throw_if_signalled();                             // Throws signal_exception if signalled() == true

  private:
    static std::atomic_int sig_code_;                 // Last sig_code which raised the signalled_ state
    static std::atomic_uint32_t sig_count_;           // Number of signals intercepted
    static std::atomic_bool signalled_;               // Whether a signal has been intercepted
    static std::function<void(int)> custom_handler_;  // Custom handling of signals
    static std::atomic_bool silent_;                  // Whether to print a message on signal interception
};

}  // namespace znode::os
