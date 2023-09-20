/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#if __has_include(<coroutine>)
#include <coroutine>

#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
namespace std {
template <typename T>
using coroutine_handle = std::experimental::coroutine_handle<T>;

using suspend_always = std::experimental::suspend_always;

using suspend_never = std::experimental::suspend_never;
}  // namespace std
#else
#error "No <coroutine> or <experimental/coroutine> include found"
#endif  // __has_include(<coroutine>)

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace zenpp {

//! Asynchronous task returned by any coroutine, i.e. asynchronous operation
template <typename T>
using Task = boost::asio::awaitable<T>;

namespace ThisTask = boost::asio::this_coro;

}  // namespace zenpp
