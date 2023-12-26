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
#include <condition_variable>
#include <mutex>

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

    //! \brief Puts the caller in wait mode for complete shutdown of this component
    void wait_stopped() noexcept;

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

  private:
    std::condition_variable component_stopped_cv_{};  // Used to signal complete shutdown of component
    std::mutex component_stopped_mutex_{};            // Guards access to component_stopped_cv
};

}  // namespace znode::con
