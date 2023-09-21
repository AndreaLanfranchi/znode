/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "shutdown_signal.hpp"

#include <infra/common/log.hpp>

namespace zenpp::cmd::common {

namespace {
    void log_signal(int signum) { log::Warning() << "Caught OS signal : " << std::to_string(signum); }
}  // namespace

using namespace zenpp::con;

void ShutDownSignal::on_signal(std::function<void(Signum)> callback) {
    signals_.async_wait([callback_{std::move(callback)}](const boost::system::error_code& error_code, int signum) {
        if (error_code) {
            std::ignore = log::Error("ShutDownSignal::on_signal", {"action", "async_wait", "error", error_code.message()});
            throw boost::system::system_error(error_code);
        }
        log_signal(signum);
        callback_(signum);
    });
}

Task<ShutDownSignal::Signum> ShutDownSignal::wait() {
    auto signal_number{co_await signals_.async_wait(boost::asio::use_awaitable)};
    log_signal(signal_number);
    co_return signal_number;
}

}  // namespace zenpp::cmd::common
