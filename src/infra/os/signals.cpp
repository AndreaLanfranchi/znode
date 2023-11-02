/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "signals.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <utility>

#include <absl/strings/str_cat.h>

namespace znode::os {

namespace {
    const char* sig_name(int sig_code) {
        switch (sig_code) {
            case SIGSEGV:
                return "SIGSEGV";
#if defined(__linux__) || defined(__APPLE__)
            case SIGBUS:
                return "SIGBUS";
            case SIGSYS:
                return "SIGSYS";
#endif
            case SIGFPE:
                return "SIGFPE";
            case SIGILL:
                return "SIGILL";
#if defined(__linux__) || defined(__APPLE__)
            case SIGTRAP:
                return "SIGTRAP";
#endif
#if defined(SIGBREAK)
            case SIGBREAK:
                return "SIGBREAK";
#endif
#if defined(__linux__) || defined(__APPLE__)
            case SIGQUIT:
                return "SIGQUIT";
#if defined(SIGSTP)
            case SIGSTP:
                return "SIGSTP";
#endif
            case SIGSTOP:
                return "SIGSTOP";
            case SIGKILL:
                return "SIGKILL";
#endif
            case SIGABRT:
                return "SIGABRT";
#if defined(SIGABRT_COMPAT)
            case SIGABRT_COMPAT:
                return "SIGABRT_COMPAT";
#endif
            case SIGINT:
                return "SIGINT";
            case SIGTERM:
                return "SIGTERM";
#if defined(__linux__) || defined(__APPLE__)
            case SIGVTALRM:
                return "SIGVTALRM";
            case SIGXFSZ:
                return "SIGXFZS";
            case SIGXCPU:
                return "SIGXCPU";
            case SIGHUP:
                return "SIGHUP";
            case SIGALRM:
                return "SIGALRM";
            case SIGUSR1:
                return "SIGUSR1";
            case SIGUSR2:
                return "SIGUSR2";
#endif
            default:
                return "Unknown";
        }
    }

    std::map<int, void (*)(int)> prev_handlers_;

}  // namespace

constexpr int kHandleableCodes[] {  // NOLINT(*-avoid-c-arrays)
#if defined(SIGBREAK)
    SIGBREAK,  // Windows keyboard CTRL+Break
#endif
#if defined(__linux__) || defined(__APPLE__)
        SIGQUIT,  // CTRL+\ (like CTRL+C but also generates a coredump)
        SIGTSTP,  // CTRL+Z to interrupt a process
#endif
        SIGINT,  // Keyboard CTRL+C
        SIGTERM  // Termination request (kill/killall default)
};

std::atomic_uint32_t Signals::sig_count_{0};
std::atomic_int Signals::sig_code_{0};
std::atomic_bool Signals::signalled_{false};
std::function<void(int)> Signals::custom_handler_;
std::atomic_bool Signals::silent_{false};

void Signals::init(std::function<void(int)> custom_handler, bool silent) {
    for (const int sig_code : kHandleableCodes) {
        // Keep track of previous handlers (if any)
        auto prev_handler{std::signal(sig_code, &Signals::handle)};
        if (prev_handler not_eq SIG_ERR) {
            prev_handlers_[sig_code] = prev_handler;
        }
    }
    custom_handler_ = std::move(custom_handler);
    silent_.exchange(silent);
}

void Signals::handle(int sig_code) {
    if (bool expected{false}; signalled_.compare_exchange_strong(expected, true)) {
        sig_code_.exchange(sig_code);
        if (not silent_)
            std::cerr << absl::StrCat("\nCaught OS signal ", sig_name(sig_code_), ", shutting down ...\n") << std::endl;
    }
    const uint32_t sig_count = ++sig_count_;
    if (sig_count >= 10) {
        std::abort();
    }
    if (sig_count > 1 and not silent_) {
        std::cerr << absl::StrCat("Already shutting down. Interrupt other ", (10 - sig_count), " times to panic.")
                  << std::endl;
    }

    if (custom_handler_) {
        custom_handler_(sig_code);
    }
    signal(sig_code, &Signals::handle);  // Re-enable the hook
}

void Signals::reset() noexcept {
    signalled_.exchange(false);
    sig_count_.exchange(0U);
    sig_code_.exchange(0);
    // Restore previous handlers
    for (const int sig_code : kHandleableCodes) {
        if (auto it{prev_handlers_.find(sig_code)}; it not_eq prev_handlers_.end()) {
            std::signal(sig_code, it->second);
        }
    }
}

void Signals::throw_if_signalled() {
    if (signalled()) {
        throw signal_exception(sig_code_);
    }
}

signal_exception::signal_exception(int code)
    : sig_code_{code}, message_{absl::StrCat("Caught OS signal ", sig_name(code))} {}
const char* signal_exception::what() const noexcept { return message_.c_str(); }

}  // namespace znode::os
