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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>

#ifdef __APPLE__
// otherwise <boost/asio/detail/socket_types.hpp> dependency doesn't compile
#define _DARWIN_C_SOURCE
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <core/common/assert.hpp>

#include <infra/concurrency/stoppable.hpp>
#include <infra/concurrency/task.hpp>

namespace znode::con {
//! \brief Implementation of an asynchronous timer relying on boost:asio
class Timer : public Stoppable {
  public:
    using duration = std::chrono::milliseconds;

    //! \brief Callback function type to be executed when interval expires
    //! \param interval [in] : length of interval between triggered events (in milliseconds)
    //! \remark The callback function can change the interval for next event
    using CallBackFunc = std::function<void(duration&)>;

    //! \brief Creates a timer without interval and callback (to be set later on start)
    //! \param executor [in] : boost's asio executor to run timer on
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param autoreset [in] : whether timer is resubmitted after callback execution
    Timer(const boost::asio::any_io_executor& executor, std::string name, bool autoreset = false)
        : timer_(executor), name_(std::move(name)), autoreset_(autoreset){};

    //! \brief Creates a timer with interval and callback
    //! \param executor [in] : boost's asio executor to run timer on
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param autoreset [in] : whether timer is resubmitted after callback execution
    //! \param interval [in] : length of interval between triggered events
    //! \param call_back [in] : the call back function to be executed when interval expires
    Timer(const boost::asio::any_io_executor& executor, std::string name, duration interval, CallBackFunc call_back,
          bool autoreset = false)
        : timer_(executor),
          name_(std::move(name)),
          autoreset_(autoreset),
          interval_(interval),
          call_back_(std::move(call_back)){};

    //! \brief Creates a timer without interval and callback (to be set later on start)
    //! \param asio_context [in] : boost's asio context to pull executor from
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param autoreset [in] : whether timer is resubmitted after callback execution
    Timer(boost::asio::io_context& context, std::string name, bool autoreset = false)
        : Timer(context.get_executor(), std::move(name), autoreset){};

    //! \brief Creates a timer with interval and callback
    //! \param executor [in] : boost's asio executor to run timer on
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param autoreset [in] : whether timer is resubmitted after callback execution
    //! \param interval [in] : length of interval between triggered events
    //! \param call_back [in] : the call back function to be executed when interval expires
    Timer(boost::asio::io_context& context, std::string name, duration interval, CallBackFunc call_back,
          bool autoreset = false)
        : Timer(context.get_executor(), std::move(name), interval, std::move(call_back), autoreset){};

    ~Timer() override = default;

    //! \brief Returns the name of the timer
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    //! \brief Returns the duration of the interval between triggered events
    //! \remarks The value returned is the one set when the timer was started. Callbacks can set new intervals
    [[nodiscard]] duration get_interval() const noexcept { return interval_.load(); }

    //! \brief Sets the the duration of interval between triggered events
    //! \remarks If timer is running and autoreset is true, the new interval is not applied until the timer is stopped
    //! and restarted
    void set_interval(duration interval) noexcept { interval_.store(interval); }

    //! \brief Returns true if timer is resubmitted after callback execution
    [[nodiscard]] bool autoreset() const noexcept { return autoreset_.load(); }

    //! \brief Sets the autoreset flag
    //! \remarks If timer is running this has no effect.
    void autoreset(bool value) noexcept { autoreset_.store(value); }

    //! \brief Sets the callback function to be executed when interval expires
    //! \remarks If timer is running this call produces no effects.
    void set_callback(CallBackFunc call_back) noexcept;

    //! \brief Starts timer and waits for interval to expire. Eventually call back action is executed and timer
    //! resubmitted for another get_interval
    bool start() noexcept override;

    //! \brief Starts timer and waits for interval to expire. Eventually call back action is executed and timer
    //! \param interval [in] : duration of wait interval
    //! \param call_back [in] : the call back function to be executed when interval expires
    bool start(duration interval, CallBackFunc call_back) noexcept;

    //! \brief Stops timer and cancels pending execution. No callback is executed and no resubmission
    bool stop() noexcept override;

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new
    //! get_interval
    void reset() { std::ignore = timer_.cancel(); }

    //! \brief Whether this timer has captured an exception
    [[nodiscard]] bool has_exception() const noexcept { return exception_ptr_ not_eq nullptr; }

    //! \brief Rethrows captured exception (if any)
    void rethrow() const;

  private:
    //! \brief Launches async timer
    Task<void> work() noexcept;

    boost::asio::steady_timer timer_;            // The timer itself
    const std::string name_{};                   // Name of the timer (for logging purposes)
    std::atomic_bool autoreset_{false};          // If true, timer is resubmitted after callback execution
    std::atomic<duration> interval_;             // Interval between triggered events
    std::function<void(duration&)> call_back_;   // Function to call on triggered
    std::atomic_bool working_{false};            // Whether the timer is working
    std::exception_ptr exception_ptr_{nullptr};  // Captured exception (if any)
};
}  // namespace znode::con
