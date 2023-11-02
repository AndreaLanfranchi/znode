/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
    thread_pool_.join();
    set_stopped();
    return true;
}
}  // namespace znode::con
