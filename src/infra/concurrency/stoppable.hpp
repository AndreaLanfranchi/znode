/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <atomic>

namespace znode::con {

//! \brief An interface providing stoppability for active components
//! \remarks It applies to in-thread components as well as to components living in their own thread
class Stoppable {
  public:
    virtual ~Stoppable() = default;

    //! \brief The state of the component
    enum class ComponentStatus {
        kNotStarted,  // Not started yet
        kStarted,     // Started and running
        kStopping,    // A stop request has been issued
    };

    //! \brief Programmatically requests the component to start
    //! \return True if the request to start has been stored otherwise false (i.e. already started)
    virtual bool start() noexcept;

    //! \brief Programmatically requests the component to stop
    //! \return True if the request to stop has been stored otherwise false (i.e. already requested to stop)
    virtual bool stop() noexcept;

    //! \brief Returns the current state of the component
    [[nodiscard]] ComponentStatus status() const noexcept;

    //! \brief Returns whether the component is running i.e. started
    [[nodiscard]] virtual bool is_running() const noexcept;

  protected:
    //! \brief This should be called by the component when, after a stop request,
    //! it has completed all outstanding tasks. This will set the component as stopped
    //! and will allow for a new start
    //! \remarks For threaded components (Worker.cpp) this is called automatically at the end of the work() function
    void set_stopped() noexcept;

    std::atomic<ComponentStatus> state_{ComponentStatus::kNotStarted};  // The state of the component
};

}  // namespace znode::con
