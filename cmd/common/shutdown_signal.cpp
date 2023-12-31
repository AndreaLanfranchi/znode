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

#include "shutdown_signal.hpp"

#include <infra/common/log.hpp>

namespace znode::cmd::common {

namespace {
    void log_signal(int signum) { log::Warning() << "Caught OS signal : " << std::to_string(signum); }
}  // namespace

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

}  // namespace znode::cmd::common
