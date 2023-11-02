/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
