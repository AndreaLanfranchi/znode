/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "detector.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <infra/common/log.hpp>

namespace zenpp::nat {
namespace {

    Task<void> resolve_local(Option& option) {
        using namespace boost::asio;
        using namespace boost::asio::ip;
        auto executor = co_await ThisTask::executor;
        udp::socket socket{executor};
        try {
            socket.connect(udp::endpoint{make_address("1.1.1.1"), 53});
            option.address_ = net::IPAddress(socket.local_endpoint().address());
            socket.close();
        } catch (const boost::system::system_error& error) {
            log::Error("Resolve local IP", {"error", error.code().message()})
                << "Are you sure you're connected to the internet ?";
        }
        co_return;
    }

    Task<void> resolve_auto(Option& option) {
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

        try {
            const auto results = co_await resolver.async_resolve(host, port, boost::asio::use_awaitable);
            if (results.empty()) throw std::runtime_error{"No results for " + host};
            co_await boost::asio::async_connect(socket, results, boost::asio::use_awaitable);

            http::request<http::string_body> request(http::verb::get, target, version);
            request.set(http::field::host, host);
            request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            http::write(socket, request);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> response;
            http::read(socket, buffer, response);

            const std::string public_ip{beast::buffers_to_string(response.body().data())};
            option.address_ = net::IPAddress::from_string(public_ip).value();

            beast::error_code error_code;
            std::ignore = socket.shutdown(tcp::socket::shutdown_both, error_code);
            if (error_code && error_code not_eq beast::errc::not_connected) {
                throw beast::system_error{error_code};
            }

        } catch (const boost::system::system_error& error) {
            log::Error("Resolve public IP", {"error", error.code().message()})
                << "Are you sure you're connected to the internet ?";
        } catch (const std::runtime_error& exception) {
            log::Error("Resolve public IP", {"error", exception.what()})
                << "Are you sure you're connected to the internet ?";
        }
        co_return;
    }

}  // namespace

Task<void> resolve(Option& option) {
    switch (option.type_) {
        using enum NatType;
        case kNone:
            co_await resolve_local(option);
            break;
        case kAuto:
            co_await resolve_auto(option);
            break;
        case kIp:
            break;
    }
    co_return;
}
}  // namespace zenpp::nat
