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

#include "task.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <catch2/catch.hpp>

namespace znode::con {

Task<int> f42() { co_return 42; }

TEST_CASE("Corouting configuration", "[infra][concurrency][coroutine]") {
#if __has_include(<coroutine>)
#ifdef BOOST_ASIO_HAS_CO_AWAIT
    CHECK(true);
#else
    CHECK(false);
#endif  // BOOST_ASIO_HAS_CO_AWAIT
#ifdef BOOST_ASIO_HAS_STD_COROUTINE
    CHECK(true);
#else
    CHECK(false);
#endif  // BOOST_ASIO_HAS_STD_COROUTINE
#endif  // __has_include(<coroutine>)
    CHECK(&typeid(std::coroutine_handle<void>) != nullptr);
    CHECK(&typeid(std::suspend_always) != nullptr);
    CHECK(&typeid(std::suspend_never) != nullptr);
}

TEST_CASE("Coroutine co_return", "[infra][concurrency][coroutine]") {
    boost::asio::io_context context;
    auto task = co_spawn(context, f42(), boost::asio::use_future);

    std::size_t work_count;
    do {
        work_count = context.poll_one();
    } while (work_count > 0);
    CHECK(task.get() == 42);
}
}  // namespace znode::con
