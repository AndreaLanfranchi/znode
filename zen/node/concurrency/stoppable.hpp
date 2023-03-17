/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <atomic>

namespace zen {

//! \brief An interface providing stoppability for active components
class Stoppable {
  public:
    //! \brief Programmatically requests the component to stop
    //! \return True if the request to stop has been stored otherwise false (i.e. already requested to stop)
    virtual bool stop(bool wait) noexcept;

    //! \brief Returns whether the component is stopping
    //! \remarks It returns true also in case an OS signal has been trapped
    [[nodiscard]] virtual bool is_stopping() const noexcept;

    virtual ~Stoppable() = default;

  private:
    std::atomic_bool stop_requested_{false};  // Whether a stop request has been issued
};

}  // namespace zen
