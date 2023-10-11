/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

// clang-format off
#include <core/common/outcome.hpp>
#include <infra/concurrency/task.hpp>
// clang-format on

#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace zenpp::con {
template <typename T>
class Channel {
  public:
    explicit Channel(boost::asio::any_io_executor executor) : channel_(std::move(executor)) {}
    Channel(boost::asio::any_io_executor executor, std::size_t max_buffer_size)
        : channel_(std::move(executor), max_buffer_size) {}

    Task<void> send(T value) {
        try {
            co_await channel_.async_send(boost::system::error_code(), value, boost::asio::use_awaitable);
        } catch (const boost::system::system_error& ex) {
            if (ex.code() == boost::asio::experimental::error::channel_cancelled)
                throw boost::system::system_error(make_error_code(boost::system::errc::operation_canceled));
            throw;
        }
    }

    bool try_send(T value) { return channel_.try_send(boost::system::error_code(), value); }

    Task<T> receive() {
        try {
            co_return (co_await channel_.async_receive(boost::asio::use_awaitable));
        } catch (const boost::system::system_error& ex) {
            if (ex.code() == boost::asio::experimental::error::channel_cancelled)
                throw boost::system::system_error(make_error_code(boost::system::errc::operation_canceled));
            throw;
        }
    }

    outcome::result<T> try_receive() {
        outcome::result<T> result{outcome::success()};
        channel_.try_receive([&](const boost::system::error_code& error, T&& value) {
            if (error)
                result = outcome::failure(error);
            else
                result = std::move(value);
        });
        return result;
    }

    void close() { channel_.close(); }

  private:
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)> channel_;
};

class EventChannel {
  public:
    explicit EventChannel(boost::asio::any_io_executor executor) : channel_{std::move(executor), 1} {}
    Task<void> wait() { co_await channel_.receive(); }
    void notify() { channel_.try_send(std::monostate{}); }

  private:
    Channel<std::monostate> channel_;
};

}  // namespace zenpp::con