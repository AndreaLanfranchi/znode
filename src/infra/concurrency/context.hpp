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
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <infra/concurrency/stoppable.hpp>

namespace znode::con {

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

}  // namespace znode::con
