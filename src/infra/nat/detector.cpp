/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "detector.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace zenpp::nat {
namespace {

    Task<net::IPAddress> detect_local() {
        using namespace boost::asio;
        using namespace boost::asio::ip;
        auto executor = co_await ThisTask::executor;
        udp::socket socket{executor};
        socket.connect(udp::endpoint{make_address("1.1.1.1"), 53});
        co_return net::IPAddress(socket.local_endpoint().address());
    }

    Task<net::IPAddress> detect_auto() {
        const std::string host{"api64.ipify.org"};
        const std::string port{"80"};
        const std::string target{"/"};
        const int version{11};

        using tcp = boost::asio::ip::tcp;

        namespace beast = boost::beast;
        namespace http = beast::http;

        auto executor = co_await ThisTask::executor;
        tcp::resolver resolver{executor};
        tcp::socket socket{executor};

        const auto results = co_await resolver.async_resolve(host, port, boost::asio::use_awaitable);
        co_await boost::asio::async_connect(socket, results, boost::asio::use_awaitable);

        http::request<http::string_body> request(http::verb::get, target, version);
        request.set(http::field::host, host);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        http::write(socket, request);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> response;
        http::read(socket, buffer, response);

        const std::string public_ip{beast::buffers_to_string(response.body().data())};

        beast::error_code error_code;
        std::ignore = socket.shutdown(tcp::socket::shutdown_both, error_code);
        if (error_code && error_code != beast::errc::not_connected) {
            throw beast::system_error{error_code};
        }

        co_return net::IPAddress::from_string(public_ip).value();
    }

}  // namespace

Task<net::IPAddress> detect(Option& option) {
    switch (option._type) {
        using enum NatType;
        case kNone:
            co_return (co_await detect_local());
        case kAuto:
            co_return (co_await detect_auto());
        case kIp:
            co_return option._address.value();
    }
    co_return net::IPAddress{};
}
}  // namespace zenpp::nat