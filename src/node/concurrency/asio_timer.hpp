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
#include <memory>
#include <utility>

#ifdef __APPLE__
// otherwise <boost/asio/detail/socket_types.hpp> dependency doesn't compile
#define _DARWIN_C_SOURCE
#endif

#include "core/common/assert.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <app/concurrency/ossignals.hpp>
#include <app/concurrency/stoppable.hpp>

namespace zenpp {
//! \brief Implementation of an asynchronous timer relying on boost:asio
class AsioTimer : public Stoppable {
  public:
    //! \brief Callback function type to be executed when get_interval expires
    //! \param interval [in] : length of wait get_interval (in milliseconds)
    //! \return 0 if timer is to be stopped, otherwise the new get_interval (in milliseconds) between triggered events
    using CallBackFunc = std::function<unsigned(unsigned)>;

    //! \param asio_context [in] : boost's asio context
    //! \param name [in] : name of the timer (for logging purposes)
    AsioTimer(boost::asio::io_context& asio_context, std::string name) : timer_(asio_context), name_(std::move(name)){};

    //! \param asio_context [in] : boost's asio context
    //! \param interval [in] : length of wait get_interval (in milliseconds)
    //! \param name [in] : name of the timer (for logging purposes)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    AsioTimer(boost::asio::io_context& asio_context, uint32_t interval, std::string name, CallBackFunc call_back)
        : timer_(asio_context),
          interval_milliseconds_(interval),
          name_(std::move(name)),
          call_back_(std::move(call_back)){};

    ~AsioTimer() override = default;

    //! \brief Returns the name of the timer
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    //! \brief Returns the get_interval (in milliseconds) between triggered events
    [[nodiscard]] uint32_t get_interval() const noexcept { return interval_milliseconds_.load(); }

    //! \brief Sets the get_interval (in milliseconds) between triggered events
    void set_interval(uint32_t interval_milliseconds) noexcept { interval_milliseconds_.store(interval_milliseconds); }

    //! \brief Returns true if timer is resubmitted after callback execution
    [[nodiscard]] bool autoreset() const noexcept { return autoreset_.load(); }

    //! \brief Sets the autoreset flag
    void set_autoreset(bool value) noexcept { autoreset_.store(value); }

    //! \brief Sets the callback function to be executed when get_interval expires
    void set_callback(CallBackFunc call_back) noexcept { call_back_ = std::move(call_back); }

    //! \brief Starts timer and waits for get_interval to expire. Eventually call back action is executed and timer
    //! resubmitted for another get_interval
    bool start() noexcept override;

    //! \param interval_milliseconds [in] : length of wait get_interval (in milliseconds)
    //! \param call_back [in] : the call back function to be executed when get_interval expires
    bool start(uint32_t interval_milliseconds, CallBackFunc call_back) noexcept;

    //! \brief Stops timer and cancels pending execution. No callback is executed and no resubmission
    bool stop([[maybe_unused]] bool wait) noexcept override;

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new
    //! get_interval
    void reset() { std::ignore = timer_.cancel(); }

  private:
    //! \brief Launches async timer
    void start_internal();

    boost::asio::steady_timer timer_;                // The timer itself
    std::atomic_uint32_t interval_milliseconds_{0};  // Interval (in milliseconds) between triggered events
    std::string name_{};                             // Name of the timer (for logging purposes)
    std::atomic_bool autoreset_{true};               // If true, timer is resubmitted after callback execution
    std::function<unsigned(unsigned)> call_back_;    // Function to call on triggered event
};
}  // namespace zenpp
