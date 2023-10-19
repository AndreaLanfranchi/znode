/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>
#include <type_traits>

// clang-format off
#include <infra/concurrency/task.hpp>
// clang-format on

#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace zenpp::con {
template <typename T>
requires std::is_copy_constructible_v<T>
class Channel {
  public:
    //! \brief Creates an instance of channel with no buffer space
    //! \details Asynchronous sends will be outstanding till a async_receive operation
    explicit Channel(boost::asio::any_io_executor executor) : channel_(std::move(executor)) {}

    //! \brief Creates an instance of channel with provided buffer space
    //! \details Asynchronous sends will be outstanding till a async_receive operation up to the fill of the buffer
    Channel(boost::asio::any_io_executor executor, std::size_t max_buffer_size)
        : channel_(std::move(executor), max_buffer_size) {}

    //! \brief Asynchronous awaitable operation to push an element into the queue
    //! \remarks The operation is always rescheduled
    //! \remarks Gets suspended when the buffer is full and as a result this will wait for a receive operation to
    //! provide room
    //! \throws boost::system::system_error in case of errors
    Task<void> async_send(T value) {
        try {
            co_await channel_.async_send(boost::system::error_code(), value, boost::asio::use_awaitable);
        } catch (const boost::system::system_error& ex) {
            if (ex.code() == boost::asio::experimental::error::channel_cancelled or
                ex.code() == boost::asio::experimental::error::channel_closed) {
                throw boost::system::system_error(make_error_code(boost::asio::error::operation_aborted));
            }
            throw;
        }
    }

    //! \brief Asynchronous awaitable operation to push an element into the queue
    //! \remarks The operation is always rescheduled
    //! \remarks Gets suspended when the buffer is full and as a result this will wait for a receive operation to
    //! provide room
    //! \remark In case of errors the error_code is set accordingly
    Task<void> async_send(boost::system::error_code& error, T value) {
        co_await channel_.async_send(error, value, boost::asio::use_awaitable);
        if (error == boost::asio::experimental::error::channel_cancelled or
            error == boost::asio::experimental::error::channel_closed) {
            error = boost::asio::error::operation_aborted;
        }
    }

    //! \brief Synchronous operation to push an element into the queue
    //! \remarks The operation returns immediately
    //! \return False when the buffer is full; True otherwise
    bool try_send(T value) { return channel_.try_send(boost::system::error_code(), value); }

    //! \brief Awaits for an item in the buffer or a async_send operation to provide one
    //! \remarks The operation is always rescheduled
    //! \remarks Gets suspended when the receive would block which means there's no element ready to pull from the
    //! buffer and as a result this will wait for a send operation to provide one \return The received element \throws
    //! boost::system::system_error in case of errors
    Task<T> async_receive() {
        try {
            co_return (co_await channel_.async_receive(boost::asio::use_awaitable));
        } catch (const boost::system::system_error& ex) {
            if (ex.code() == boost::asio::experimental::error::channel_cancelled or
                ex.code() == boost::asio::experimental::error::channel_closed) {
                throw boost::system::system_error(make_error_code(boost::asio::error::operation_aborted));
            }
            throw;
        }
    }

    //! \brief Awaits for an item in the buffer or a async_send operation to provide one
    //! \remarks The operation is always rescheduled
    //! \remarks Gets suspended when the receive would block which means there's no element ready to pull from the
    //! buffer and as a result this will wait for a send operation to provide one \returns An optional with the received
    //! element or std::nullopt in case of errors (which are stored in the error_code)
    Task<std::optional<T>> async_receive(boost::system::error_code& error) {
        std::optional<T> ret{std::nullopt};
        try {
            ret.emplace(co_await channel_.async_receive(boost::asio::use_awaitable));
        } catch (const boost::system::system_error& ex) {
            if (ex.code() == boost::asio::experimental::error::channel_cancelled or
                ex.code() == boost::asio::experimental::error::channel_closed) {
                error = boost::asio::error::operation_aborted;
            } else {
                error = ex.code();
            }
        }
        co_return ret;
    }

    //! \brief Synchronous operation to try receive an element
    //! \remarks The operation returns immediately
    //! \return False when there are no elements in the buffer; True otherwise
    bool try_receive(T& result) {
        if (not channel_.is_open() or not channel_.ready()) return false;
        boost::system::error_code err;
        channel_.try_receive([&err, &result](const boost::system::error_code& error, T&& value) {
            if (error)
                err = error;
            else
                result = std::move(value);
        });
        return not err;
    }

    //! \brief Determine whether the channel is open
    [[nodiscard]] bool is_open() const noexcept { return channel_.is_open(); }

    //! \brief Determine whether an element can be received without blocking
    [[nodiscard]] bool ready() const noexcept { return channel_.ready(); }

    //! \brief Closes the channel
    void close() { channel_.close(); }

  private:
    boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)> channel_;
};

class NotifyChannel {
  public:
    explicit NotifyChannel(boost::asio::any_io_executor executor) : channel_{std::move(executor), 1} {}
    Task<void> wait_one() { co_await channel_.async_receive(); }
    bool notified() { 
        std::monostate result;
        return channel_.try_receive(result); 
    }
    void notify() { std::ignore = channel_.try_send(std::monostate{}); }

  private:
    Channel<std::monostate> channel_;
};

}  // namespace zenpp::con