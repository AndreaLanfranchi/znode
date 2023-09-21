/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/signal_set.hpp>

#include <infra/concurrency/task.hpp>

namespace zenpp::cmd::common {

class ShutDownSignal {
  public:
    explicit ShutDownSignal(boost::asio::any_io_executor executor) : signals_(executor, SIGINT, SIGTERM){};
    ~ShutDownSignal() = default;

    using Signum = int;
    void on_signal(std::function<void(Signum)> callback);
    zenpp::con::Task<Signum> wait();

  private:
    boost::asio::signal_set signals_;
};

}  // namespace zenpp::cmd::common
