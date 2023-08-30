/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <atomic>

namespace zenpp {

//! \brief An interface providing stoppability for active components
class Stoppable {
  public:
    virtual ~Stoppable() = default;

    //! \brief Programmatically requests the component to start
    //! \return True if the request to start has been stored otherwise false (i.e. already started)
    virtual bool start() noexcept;

    //! \brief Programmatically requests the component to stop
    //! \return True if the request to stop has been stored otherwise false (i.e. already requested to stop)
    virtual bool stop(bool wait) noexcept;

    //! \brief Returns whether the component is stopping
    //! \remarks It returns true also in case an OS signal has been trapped
    [[nodiscard]] virtual bool is_stopping() const noexcept;

    //! \brief Returns whether the component is running (i.e. started and not requested to stop)
    [[nodiscard]] virtual bool is_running() const noexcept;

    //! \brief This should be called by the component when, after a stop request,
    //! it has completed all outstanding tasks. This will set the component as stopped
    //! and will allow for a new start
    void set_stopped() noexcept;

  private:
    std::atomic_bool started_{false};         // Whether the component has been started
    std::atomic_bool stop_requested_{false};  // Whether a stop request has been issued
};

}  // namespace zenpp
