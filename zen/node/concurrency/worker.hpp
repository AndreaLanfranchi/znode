/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <atomic>
#include <condition_variable>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/signals2/signal.hpp>

#include <zen/node/concurrency/stoppable.hpp>

namespace zen {

//! \brief An active component living in its own thread with stoppable features
//! \remarks Can also stay im non-busy wait for new work to be done
class Worker : public Stoppable, private boost::noncopyable {
  public:
    //! \brief Actually the state of the underlying thread
    enum class State {
        kStopped,
        kStarting,
        kStarted,
        kKickWaiting,
        kStopping
    };

    Worker() : name_{"worker"} {}
    explicit Worker(const std::string& name) : name_{name} {}
    explicit Worker(std::string&& name) : name_{std::move(name)} {}

    ~Worker() override;

    virtual void start(bool kicked,
                       bool wait) noexcept;  // Start worker thread with provided kicked state (optionally waits
                                             // for thread started)

    bool stop(bool wait) noexcept override;  // Stops worker thread (optionally wait for join)
    void kick();                             // Kicks worker thread if in waiting state

    //! \brief Returns the name of this worker
    std::string name() const { return name_; }

    //! \brief Returns the id of this worker (is the thread id)
    size_t id() const {
        if (!thread_) return 0;
        return std::hash<std::thread::id>{}(thread_->get_id());
    }

    //! \brief Retrieves current state of thread
    State state() const noexcept { return state_.load(std::memory_order_relaxed); }

    //! \brief Whether this worker/thread has encountered an exception
    bool has_exception() const noexcept { return exception_ptr_ != nullptr; }

    //! \brief Returns the message of captured exception (if any)
    std::string what() const noexcept;

    //! \brief Rethrows captured exception (if any)
    void rethrow() const;

    //! \brief Signals connected handlers about worker's state changes
    boost::signals2::signal<void(Worker* sender)> signal_worker_state_changed;

  protected:
    /**
     * @brief Puts the underlying thread in non-busy wait for a kick
     * to waken up and do work.
     * Returns True if the kick has been received and should go ahead
     * otherwise False (i.e. the thread has been asked to stop)
     *
     * @param [in] timeout: Timeout for conditional variable wait (milliseconds). Defaults to 100 ms
     */
    bool wait_for_kick(uint32_t timeout_milliseconds = 100);  // Puts a thread in non-busy wait for data to process
    std::atomic_bool kicked_{false};                          // Whether the kick has been received
    std::condition_variable kicked_cv_{};                     // Condition variable to wait for kick
    std::mutex kick_mtx_{};                                   // Mutex for conditional wait of kick
    std::string name_;

  private:
    std::atomic_uint64_t id_{0};  // Obtained from thread_id
    std::atomic<State> state_{State::kStopped};
    std::unique_ptr<std::thread> thread_{nullptr};
    std::condition_variable thread_started_cv_{};
    std::exception_ptr exception_ptr_{nullptr};
    virtual void work() = 0;  // Derived classes must override
};

}  // namespace zen
