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
#include <utility>

#ifdef __APPLE__
// otherwise <boost/asio/detail/socket_types.hpp> dependency doesn't compile
#define _DARWIN_C_SOURCE
#endif

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>

#include <zen/core/common/assert.hpp>

#include <zen/node/concurrency/ossignals.hpp>

namespace zen {
//! \brief Implementation of an asynchronous timer relying on boost:asio
class Timer {
  public:
    //! \param asio_context [in] : boost's asio context
    //! \param interval [in] : length of wait interval (in milliseconds)
    //! \param call_back [in] : the call back function to be called
    //! \param auto_start [in] : whether to start the timer immediately
    explicit Timer(boost::asio::io_context& asio_context, uint32_t interval, std::function<bool()> call_back,
                   bool auto_start = false)
        : interval_(interval), timer_(asio_context), call_back_(std::move(call_back)) {
        ZEN_ASSERT(interval > 0);
        if (auto_start) {
            start();
        }
    };

    ~Timer() { stop(); }

    //! \brief Starts timer and waits for interval to expire. Eventually call back action is executed and timer
    //! resubmitted for another interval
    void start() {
        bool expected_running{false};
        if (is_running.compare_exchange_strong(expected_running, true)) {
            launch();
        }
    }

    //! \brief Stops timer and cancels pending execution. No callback is executed and no resubmission
    void stop() {
        bool expected_running{true};
        if (is_running.compare_exchange_strong(expected_running, false)) {
            (void)timer_.cancel();
        }
    }

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new interval
    void reset() { (void)timer_.cancel(); }

  private:
    //! \brief Launches async timer
    void launch() {
        timer_.expires_from_now(boost::posix_time::milliseconds(interval_));
        (void)timer_.async_wait([&, this](const boost::system::error_code& ec) {
            if (!ec && call_back_) {
                call_back_();
            }
            if (is_running.load()) {
                launch();
            }
        });
    }

    std::atomic_bool is_running{false};
    const uint32_t interval_;
    boost::asio::deadline_timer timer_;
    std::function<bool()> call_back_;
};
}  // namespace zen
