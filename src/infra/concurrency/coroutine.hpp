/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace zenpp {

//! Asynchronous task returned by any coroutine, i.e. asynchronous operation
template <typename T>
using Task = boost::asio::awaitable<T>;

namespace ThisTask = boost::asio::this_coro;

}  // namespace zenpp
