/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "context.hpp"

#include <thread>

#include <boost/asio/post.hpp>

#include <infra/common/log.hpp>

namespace zenpp::con {

using namespace boost;
Context::Context(std::string name, size_t concurrency)
    : name_{std::move(name)},
      concurrency_(concurrency),
      io_context_{std::make_unique<asio::io_context>(static_cast<int>(concurrency))},
      work_guard_(asio::make_work_guard(*io_context_)),
      thread_pool_(concurrency) {}

bool Context::start() noexcept {
    if (not Stoppable::start()) return false;  // Already started
    LOG_TRACE << "Starting [" << name_ << "] context with " << concurrency_ << " threads";
    for (size_t i{0U}; i < concurrency_; ++i) {
        asio::post(thread_pool_, [this] {
            LOG_TRACE << "Starting thread " << std::this_thread::get_id() << " in context ";
            io_context_->run();
        });
    }
    return true;
}
bool Context::stop(bool wait) noexcept {
    if (not Stoppable::stop(wait)) return false;  // Already stopped
    LOG_TRACE << "Stopping [" << name_ << "] context";
    work_guard_.reset();
    io_context_->stop();
    thread_pool_.join();
    set_stopped();
    return true;
}
}  // namespace zenpp::con
