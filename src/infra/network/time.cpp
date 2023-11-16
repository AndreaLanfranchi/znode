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

#include "time.hpp"

#include <array>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include <core/common/endian.hpp>
#include <core/common/time.hpp>

#include <infra/common/log.hpp>
#include <infra/network/errors.hpp>

namespace znode::net {

outcome::result<void> check_system_time(boost::asio::any_io_executor executor, const std::string& time_server,
                                        uint32_t max_skew_seconds) {
    using namespace boost::asio;
    using namespace boost::asio::ip;
    using namespace std::chrono_literals;

    tcp::resolver resolver(executor);
    boost::system::error_code error_code;
    const auto dns_entries{resolver.resolve(time_server, "", error_code)};
    if (error_code) {
        return outcome::failure(error_code);
    }

    const auto& time_server_adress = dns_entries.begin()->endpoint().address();
    udp::endpoint receiver_endpoint(time_server_adress, 123);
    udp::socket socket(executor);
    std::ignore = socket.open(receiver_endpoint.protocol(), error_code);
    if (error_code) {
        return outcome::failure(error_code);
    }

    std::array<unsigned char, 48> send_buf = {0x00};
    std::array<unsigned char, 48> recv_buf = {0x00};
    send_buf[0] = 0x1B;  // NTP version

    socket.send_to(buffer(send_buf), receiver_endpoint, 0, error_code);
    if (error_code) {
        return outcome::failure(error_code);
    }

    // Get the system time
    const std::time_t system_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    udp::endpoint sender_endpoint;
    size_t len = socket.receive_from(buffer(recv_buf), sender_endpoint, 0, error_code);
    if (error_code) {
        return outcome::failure(error_code);
    }
    if (len != 48) {
        return Error::kInvalidNtpResponse;
    }

    // Interpret the ntp packet
    auto transmitted_time = endian::load_big_u32(&recv_buf[40]);
    transmitted_time -= 2208988800U;  // NTP time starts in 1900, UNIX in 1970
    std::ignore = log::Message(
        "Time Sync", {time_server, format_ISO8601(transmitted_time), "system", format_ISO8601(system_time_t)});

    if (max_skew_seconds not_eq 0U) {
        const auto delta_time = static_cast<uint32_t>(std::abs(std::difftime(system_time_t, transmitted_time)));
        if (delta_time > max_skew_seconds) {
            std::ignore = log::Error("Time Sync", {"skew seconds", std::to_string(delta_time), "max skew seconds",
                                                   std::to_string(max_skew_seconds)});
            return Error::kInvalidSystemTime;
        }
    }
    return outcome::success();
}
}  // namespace znode::net
