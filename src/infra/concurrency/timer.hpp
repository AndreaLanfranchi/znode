/*
Copyright 2022 The Silkworm Authors
Copyright 2023 Horizen Labs
Distributed under the MIT software license, see the accompanying
file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>

#ifdef __APPLE__
// otherwise <boost/asio/detail/socket_types.hpp> dependency doesn't compile
#define _DARWIN_C_SOURCE
#endif

#include <absl/functional/function_ref.h>
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

    //! \param asio_context [in] : boost's asio context
    //! \param name [in] : name of the timer (for logging purposes)
    Timer(boost::asio::io_context& asio_context, std::string name, bool autoreset = false)
        : timer_(asio_context), name_(std::move(name)), autoreset_(autoreset){};

    //! \param asio_context [in] : boost's asio context
    //! \param interval [in] : length of wait get_interval (in milliseconds)
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    Timer(boost::asio::io_context& asio_context, std::chrono::milliseconds interval, std::string name,
          CallBackFunc call_back, bool autoreset = false)
        : timer_(asio_context),
          interval_(interval),
          name_(std::move(name)),
          autoreset_(autoreset),
          call_back_(std::move(call_back)){};

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
    void set_autoreset(bool value) noexcept {
        if (not is_running()) autoreset_.store(value);
    }

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
    bool stop([[maybe_unused]] bool wait) noexcept override;

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new
    //! get_interval
    void reset() { std::ignore = timer_.cancel(); }

    //! \brief Whether this timer has captured an exception
    bool has_exception() const noexcept { return exception_ptr_ not_eq nullptr; }

    //! \brief Rethrows captured exception (if any)
    void rethrow() const;

  private:
    //! \brief Launches async timer
    Task<void> start_detached() noexcept;

    boost::asio::steady_timer timer_;                  // The timer itself
    std::atomic<std::chrono::milliseconds> interval_;  // Interval between triggered events
    std::string name_{};                               // Name of the timer (for logging purposes)
    std::atomic_bool autoreset_{false};                // If true, timer is resubmitted after callback execution
    std::function<void(std::chrono::milliseconds&)> call_back_;  // Function to call on triggered
    std::condition_variable stop_cv_{};                          // Condition variable to wait for on
    std::mutex stop_mtx_{};                                      // Mutex for conditional wait of kick
    std::exception_ptr exception_ptr_{nullptr};                  // Captured exception (if any)
};
}  // namespace zenpp::con
