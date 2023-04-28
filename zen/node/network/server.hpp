/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/noncopyable.hpp>

#include <zen/node/common/log.hpp>
#include <zen/node/concurrency/stoppable.hpp>

namespace zen::network {

class Server final : public zen::Stoppable, private boost::noncopyable {
  public:
    Server(boost::asio::io_context& io_context, boost::asio::ip::tcp::endpoint endpoint)
        : io_context_{io_context}, acceptor_{io_context, endpoint} {}
    ~Server() override = default;

    void start() {
        boost::asio::spawn(io_context_, [this](boost::asio::yield_context yield) {
            while (!is_stopping()) {
                boost::system::error_code ec;
                boost::asio::ip::tcp::socket socket{io_context_};
                acceptor_.async_accept(socket, yield[ec]);
                if (ec == boost::asio::error::operation_aborted) break;  // acceptor closed by programmatic request

                // TODO determine whether we can accept this new inbound connection or
                // we have exceeded the maximum number of connections

                if (!ec) {
                    log::Info("Accepted connection", {"ip", socket.remote_endpoint().address().to_string()});
                    boost::asio::spawn(
                        io_context_, [this, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                            try {
                                std::array<char, 1024> buffer;
                                boost::system::error_code ec1;
                                size_t len{0};
                                while (!is_stopping() &&
                                       (len = socket.async_read_some(boost::asio::buffer(buffer), yield[ec1])) > 0 &&
                                       !ec1) {
                                    boost::asio::async_write(socket, boost::asio::buffer(buffer, len), yield[ec1]);
                                    if (ec1) break;
                                }
                            } catch (const std::exception& e) {
                                log::Error("Connection acceptor", {"status", "aborted", "reason", e.what()});
                                std::ignore = Stoppable::stop(false);
                            }
                        });
                } else {
                    log::Error("Connection acceptor", {"status", "aborted", "reason", ec.message()});
                }
            }
        });
    }

    bool stop(bool wait) override {
        bool ret = Stoppable::stop(wait);
        if (ret) {
            acceptor_.cancel();
        }
    }

  private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

}  // namespace zen::network
