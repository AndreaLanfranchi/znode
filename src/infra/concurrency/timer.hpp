/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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

namespace zenpp::con {
//! \brief Implementation of an asynchronous timer relying on boost:asio
class Timer : public Stoppable {
  public:
    //! \brief Callback function type to be executed when get_interval expires
    //! \param interval [in] : length of interval between triggered events (in milliseconds)
    //! \remark The callback function can change the interval for next event
    using CallBackFunc = std::function<void(std::chrono::milliseconds&)>;

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
    //! \param interval [in] : length of interval between triggered events (in milliseconds)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    Timer(const boost::asio::any_io_executor& executor, std::string name, std::chrono::milliseconds interval,
          CallBackFunc call_back, bool autoreset = false)
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
    //! \param interval [in] : length of interval between triggered events (in milliseconds)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    Timer(boost::asio::io_context& context, std::string name, std::chrono::milliseconds interval,
          CallBackFunc call_back, bool autoreset = false)
        : Timer(context.get_executor(), std::move(name), interval, std::move(call_back), autoreset){};

    ~Timer() override = default;

    //! \brief Returns the name of the timer
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    //! \brief Returns the get_interval (in milliseconds) between triggered events
    //! \remarks The value returned is the one set when the timer was started. Callbacks can set new intervals
    [[nodiscard]] std::chrono::milliseconds get_interval() const noexcept { return interval_.load(); }

    //! \brief Sets the get_interval (in milliseconds) between triggered events
    //! \remarks If timer is running and autoreset is true, the new interval is not applied until the timer is stopped
    //! and restarted
    void set_interval(std::chrono::milliseconds interval) noexcept { interval_.store(interval); }

    //! \brief Returns true if timer is resubmitted after callback execution
    [[nodiscard]] bool autoreset() const noexcept { return autoreset_.load(); }

    //! \brief Sets the autoreset flag
    //! \remarks If timer is running this has no effect.
    void autoreset(bool value) noexcept { autoreset_.store(value); }

    //! \brief Sets the callback function to be executed when get_interval expires
    //! \remarks If timer is running this call produces no effects.
    void set_callback(CallBackFunc call_back) noexcept;

    //! \brief Starts timer and waits for get_interval to expire. Eventually call back action is executed and timer
    //! resubmitted for another get_interval
    bool start() noexcept override;

    //! \param interval_milliseconds [in] : length of wait get_interval (in milliseconds)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    bool start(std::chrono::milliseconds interval, CallBackFunc call_back) noexcept;

    //! \brief Stops timer and cancels pending execution. No callback is executed and no resubmission
    bool stop() noexcept override;

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new
    //! get_interval
    void reset() { std::ignore = timer_.cancel(); }

    //! \brief Whether this timer has captured an exception
    bool has_exception() const noexcept { return exception_ptr_ not_eq nullptr; }

    //! \brief Rethrows captured exception (if any)
    void rethrow() const;

  private:
    //! \brief Launches async timer
    Task<void> work() noexcept;

    boost::asio::steady_timer timer_;                  // The timer itself
    const std::string name_{};                         // Name of the timer (for logging purposes)
    std::atomic_bool autoreset_{false};                // If true, timer is resubmitted after callback execution
    std::atomic<std::chrono::milliseconds> interval_;  // Interval between triggered events
    std::function<void(std::chrono::milliseconds&)> call_back_;  // Function to call on triggered
    std::atomic_bool working_{false};                            // Whether the timer is working
    std::exception_ptr exception_ptr_{nullptr};                  // Captured exception (if any)
};
}  // namespace zenpp::con
