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

#include "context.hpp"

#include <thread>

#include <absl/strings/str_cat.h>
#include <boost/asio/post.hpp>

#include <infra/common/log.hpp>

namespace znode::con {

using namespace boost;
Context::Context(std::string name, size_t concurrency)
    : name_{std::move(name)},
      concurrency_(concurrency),
      io_context_{std::make_unique<asio::io_context>(static_cast<int>(concurrency))},
      work_guard_(asio::make_work_guard(*io_context_)),
      thread_pool_(concurrency) {}

bool Context::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started
    LOG_TRACE2 << "Starting [" << name_ << "] context with " << concurrency_ << " threads";
    for (size_t i{0U}; i < concurrency_; ++i) {
        asio::post(thread_pool_, [this, i] {
            const std::string thread_name{absl::StrCat("asio-", name_, "-", i)};
            LOG_TRACE2 << "Starting thread " << thread_name << " in context ";
            log::set_thread_name(thread_name);
            io_context_->run();
            LOG_TRACE2 << "Stopping thread " << thread_name << " in context ";
        });
    }
    return true;
}

bool Context::stop() noexcept {
    if (not Stoppable::stop()) return false;  // Already stopped
    LOG_TRACE2 << "Stopping [" << name_ << "] context";
    work_guard_.reset();
    io_context_->stop();
    thread_pool_.stop();
    thread_pool_.join();
    set_stopped();
    return true;
}
}  // namespace znode::con
