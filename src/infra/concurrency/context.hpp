/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <infra/concurrency/stoppable.hpp>

namespace zenpp::con {

//! \brief Context is a wrapper around boost::asio::io_context
class Context final : public Stoppable {
  public:
    explicit Context(std::string name, size_t concurrency = 1U);
    ~Context() { std::ignore = stop(); }

    // Not copyable nor movable
    Context(const Context& other) = delete;
    Context(Context&& other) = delete;

    [[nodiscard]] boost::asio::io_context* get() const { return io_context_.get(); }
    [[nodiscard]] boost::asio::io_context* operator->() const { return io_context_.get(); }
    [[nodiscard]] boost::asio::io_context& operator*() const { return *io_context_; }
    [[nodiscard]] boost::asio::any_io_executor executor() const { return io_context_->get_executor(); }

    bool start() noexcept override;
    bool stop() noexcept override;

  private:
    const std::string name_;    // Name of the context
    const size_t concurrency_;  // Level of concurrency
    std::unique_ptr<boost::asio::io_context> io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::thread_pool thread_pool_;
};

}  // namespace zenpp::con
