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

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <app/concurrency/ossignals.hpp>
#include <app/concurrency/stoppable.hpp>

#include "core/common/assert.hpp"

namespace zenpp {
//! \brief Implementation of an asynchronous timer relying on boost:asio
class AsioTimer : public Stoppable, public std::enable_shared_from_this<AsioTimer> {
  public:
    //! \param asio_context [in] : boost's asio context
    //! \param interval [in] : length of wait interval (in milliseconds)
    //! \param call_back [in] : the call back function to be executed when interval expires
    AsioTimer(boost::asio::io_context& asio_context, uint32_t interval, std::function<bool()> call_back)
        : timer_(asio_context), interval_(interval), call_back_(std::move(call_back)) {
        REQUIRES(interval > 0);
    };

    ~AsioTimer() override = default;

    //! \brief Starts timer and waits for interval to expire. Eventually call back action is executed and timer
    //! resubmitted for another interval
    bool start() noexcept override {
        if (Stoppable::start()) {
            start_timer();
            return true;
        }
        return false;
    }

    //! \brief Stops timer and cancels pending execution. No callback is executed and no resubmission
    bool stop([[maybe_unused]] bool wait) noexcept override {
        if (Stoppable::stop(wait)) {
            std::ignore = timer_.cancel();
            return true;
        }
        return false;
    }

    //! \brief Cancels execution of awaiting callback and, if still in running state, submits timer for a new interval
    void reset() { std::ignore = timer_.cancel(); }

  private:
    //! \brief Launches async timer
    void start_timer() {
        timer_.expires_after(std::chrono::milliseconds(interval_));
        (void)timer_.async_wait([self{shared_from_this()}](const boost::system::error_code& error_code) {
            if (self->do_call_back(error_code)) {
                self->start_timer();
            } else {
                self->set_stopped();
            }
        });
    }

    //! \brief Executes the callback function
    bool do_call_back(boost::system::error_code error_code) {
        bool ret{error_code == boost::asio::error::operation_aborted or is_stopping()};
        try {
            if (ret) ret = call_back_();
        } catch (...) {
            ret = false;
        }
        return (ret and not is_stopping());
    }

    boost::asio::steady_timer timer_;  // The timer itself
    const uint32_t interval_;          // Interval (in milliseconds) between triggered events
    std::function<bool()> call_back_;  // Function to call on triggered event
};
}  // namespace zenpp
