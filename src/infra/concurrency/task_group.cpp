/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "task_group.hpp"

#include <tuple>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/system_error.hpp>

#include <core/common/assert.hpp>
#include <infra/common/log.hpp>

namespace zenpp::con {
namespace {
    bool is_operation_cancelled_exception(const std::exception_ptr& ex_ptr) {
        try {
            std::rethrow_exception(ex_ptr);
        } catch (const boost::system::system_error& exception) {
            return exception.code() == boost::system::errc::operation_canceled;
        } catch (...) {
            return false;
        }
    }
}  // namespace
using namespace boost;

void TaskGroup::spawn(asio::any_io_executor executor, Task<void> task) {
    std::scoped_lock lock(mutex_);
    if (closed_) {
        throw std::runtime_error("TaskGroup is closed");
    }
    auto task_id = ++last_task_id_;
    auto [it, inserted] =
        tasks_.emplace(std::piecewise_construct, std::forward_as_tuple(task_id), std::forward_as_tuple());
    ASSERT_POST(inserted);

    auto cancellation_slot = it->second.slot();
    auto completion_handler = [this, task_id](const std::exception_ptr& ex_ptr) { this->on_task_completed(task_id, ex_ptr); };
    asio::co_spawn(executor, std::move(task), asio::bind_cancellation_slot(cancellation_slot, completion_handler));
}

Task<void> TaskGroup::wait() { 

    // wait until cancelled or a task throws an exception
    std::exception_ptr ex_ptr;
    try {
        ex_ptr = co_await exceptions_.async_receive();
    } catch (const boost::system::system_error& ex) {
        if (ex.code() == boost::system::errc::operation_canceled) {
            ex_ptr = std::current_exception();
        } else {
            log::Error() << "TaskGroup::wait system_error: " << ex.what();
            throw;
        }
    }

    co_await ThisTask::reset_cancellation_state();
    close();

        // wait for all tasks completions
    while (not completed()) {
        auto [completed_task_id, result_ex_ptr] = co_await completions_.async_receive();

        {
            std::scoped_lock lock(mutex_);
            tasks_.erase(completed_task_id);
        }

        if (result_ex_ptr) {
            ex_ptr = result_ex_ptr;
        }
    }

    std::rethrow_exception(ex_ptr);
}

void TaskGroup::close() {

    std::scoped_lock lock(mutex_);
    closed_ = true;
    for (auto& [task_id, canceller] : tasks_) {
        canceller.emit(asio::cancellation_type::all);
    }
}

void TaskGroup::on_task_completed(std::size_t task_id, const std::exception_ptr& ex_ptr) {
    const bool is_cancelled{ex_ptr not_eq nullptr and is_operation_cancelled_exception(ex_ptr)};
    std::scoped_lock lock(mutex_);
    if (closed_) {

        // If a task threw (not due to cancel) rethrow later
        std::exception_ptr effective_ex_ptr{ex_ptr not_eq nullptr and not is_cancelled ? ex_ptr : nullptr};
        if (not completions_.try_send({task_id, effective_ex_ptr})) {
            throw std::runtime_error("TaskGroup::on_task_completed completion queue is unexpectedly full");
        }
        return;
    }

    tasks_.erase(task_id);
    if (not is_cancelled and ex_ptr not_eq nullptr) {
        std::ignore = exceptions_.try_send(ex_ptr);
    }
}

bool TaskGroup::completed() {
    std::scoped_lock lock(mutex_);
    return closed_ and tasks_.empty();
}
}  // namespace zenpp::con
