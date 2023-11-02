/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <atomic>
#include <condition_variable>
#include <optional>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include <infra/concurrency/stoppable.hpp>

namespace znode::con {

//! \brief An active component living in its own thread with stoppable features
//! \remarks Can also stay im non-busy wait for new work to be done
class Worker : public Stoppable, private boost::noncopyable {
  public:
    Worker() : name_{"worker"} {}
    explicit Worker(const std::string& name, std::optional<size_t> stack_size = std::nullopt)
        : name_{name}, stack_size_{stack_size} {}
    explicit Worker(std::string&& name, std::optional<size_t> stack_size = std::nullopt)
        : name_{std::move(name)}, stack_size_{stack_size} {}

    ~Worker() override;

    //! \brief Starts worker thread
    bool start() noexcept override;

    //! \brief Stops worker thread
    bool stop() noexcept override;

    //! \brief Wakes up worker thread to do work
    void kick();

    //! \brief Returns the name of this worker
    [[nodiscard]] std::string name() const { return name_; }

    //! \brief Returns the id of this worker (is the thread id)
    [[nodiscard]] uint64_t id() const { return id_; }

    //! \brief Whether this worker/thread has encountered an exception
    bool has_exception() const noexcept { return exception_ptr_ not_eq nullptr; }

    //! \brief Returns the message of captured exception (if any)
    std::string what() const noexcept;

    //! \brief Rethrows captured exception (if any)
    void rethrow() const;

  protected:
    /**
     * @brief Puts the underlying thread in non-busy wait for a kick
     * to waken up and do work.
     * Returns True if the kick has been received and should go ahead
     * otherwise False (i.e. the thread has been asked to stop)
     *
     * @param [in] timeout: Timeout for conditional variable wait (milliseconds). Defaults to 100 ms
     */
    bool wait_for_kick(uint32_t timeout_milliseconds = 1'000);  // Puts a thread in non-busy wait for data to process
    std::atomic_bool kicked_{false};                            // Whether the kick has been received
    std::condition_variable kicked_cv_{};                       // Condition variable to wait for kick
    std::mutex kick_mtx_{};                                     // Mutex for conditional wait of kick
    std::string name_;

  private:
    std::optional<size_t> stack_size_{};
    std::atomic_uint64_t id_{0};  // Obtained from thread_id
    std::unique_ptr<boost::thread> thread_{nullptr};
    std::exception_ptr exception_ptr_{nullptr};
    virtual void work() = 0;  // Derived classes must override
};

}  // namespace znode::con
