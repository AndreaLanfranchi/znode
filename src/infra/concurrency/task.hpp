/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <coroutine>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace zenpp {

//! \brief Asynchronous task returned by any coroutine, i.e. asynchronous operation
template <typename T = void, typename Executor = boost::asio::any_io_executor>
using Task = boost::asio::awaitable<T, Executor>;

namespace ThisTask = boost::asio::this_coro;

}  // namespace zenpp
