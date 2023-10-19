/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>

#include <boost/asio/any_io_executor.hpp>
#include <boost/noncopyable.hpp>

#include <infra/concurrency/channel.hpp>
#include <infra/concurrency/task.hpp>

namespace zenpp::con {

class TaskGroup : public boost::noncopyable {
  public:
    TaskGroup(boost::asio::any_io_executor executor, std::size_t max_tasks)
        : completions_(executor, max_tasks), exceptions_(executor, 1U) {}

    void spawn(boost::asio::any_io_executor executor, Task<void> task);
    Task<void> wait();

  private:
    void close();
    void on_task_completed(std::size_t task_id, const std::exception_ptr& ex_ptr);
    bool completed();

    std::mutex mutex_;
    bool closed_{false};
    std::size_t last_task_id_{0};
    std::map<std::size_t, boost::asio::cancellation_signal> tasks_;
    Channel<std::pair<std::size_t, std::exception_ptr>> completions_;
    Channel<std::exception_ptr> exceptions_;
};

}  // namespace zenpp::con
