/*
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

#include "detector.hpp"

#include "core/common/endian.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <infra/common/log.hpp>
#include <infra/common/random.hpp>

namespace znode::nat {
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

    Task<void> resolve_stun(Option& option) {
        const std::string host{"stun.l.google.com"};
        const std::string port{"19302"};

        using namespace boost::asio;
        using namespace boost::asio::ip;
        auto executor = co_await ThisTask::executor;
        udp::resolver resolver{executor};
        udp::socket socket{executor};
        boost::system::error_code error_code;

        try {
            const auto results{resolver.resolve(host, port, error_code)};
            LOGF_MESSAGE << "STUN results: " << results.size();
            if (error_code) throw boost::system::system_error{error_code};
            if (results.empty()) throw std::runtime_error{"No DNS results for " + host};

            boost::asio::streambuf request;
            std::ostream request_stream{&request};

            // Prepare the request
            auto message{get_random_bytes(24)};
            message[0] = 0x00;  // > binding request
            message[1] = 0x01;
            message[2] = 0x00;
            message[3] = 0x00;  // < binding request
            message[4] = 0x21;  // > magic cookie
            message[5] = 0x12;
            message[6] = 0xA4;
            message[7] = 0x42;  // < magic cookie

            request_stream.write(reinterpret_cast<const char*>(message.data()), message.size());

            const udp::endpoint receiver_endpoint = results.begin()->endpoint();
            std::ignore = socket.open(receiver_endpoint.protocol(), error_code);
            if (error_code) throw boost::system::system_error{error_code};

            co_await socket.async_send_to(request.data(), receiver_endpoint, redirect_error(use_awaitable, error_code));
            LOGF_MESSAGE << "STUN request sent: " << receiver_endpoint;
            if (error_code) throw boost::system::system_error{error_code};

            udp::endpoint local_endpoint{receiver_endpoint.protocol(), 0};
            boost::asio::streambuf response;
            co_await socket.async_receive_from(response.prepare(64_KiB), local_endpoint, 0,
                                               redirect_error(use_awaitable, error_code));
            LOGF_MESSAGE << "STUN response received";
            if (error_code) throw boost::system::system_error{error_code};

            if (reinterpret_cast<const unsigned int*>(response.data().data())[5] == 0x001U) {
                // IPv4
                const auto* data = reinterpret_cast<const unsigned char*>(response.data().data());
                ip::address_v4 ip(endian::load_little_u32(&data[28]));
                option.address_ = net::IPAddress(ip);
            } else {
                // IPv6
                const auto* data = reinterpret_cast<const unsigned char*>(response.data().data());
                ip::address_v6::bytes_type ip_bytes;
                std::copy(&data[28], &data[44], ip_bytes.begin());
                ip::address_v6 ip(ip_bytes);
                option.address_ = net::IPAddress(ip);
            }
        } catch (const boost::system::system_error& error) {
            log::Error("Resolve STUN IP", {"error", error.code().message()})
                << "Are you sure you're connected to the internet ?";
        } catch (const std::runtime_error& exception) {
            log::Error("Resolve STUN IP", {"error", exception.what()})
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
            if (results.empty()) throw std::runtime_error{"No DNS results for " + host};
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
        case kStun:
            co_await resolve_stun(option);
            break;
        case kIp:
            break;
    }
    co_return;
}
}  // namespace znode::nat
