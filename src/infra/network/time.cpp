/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "time.hpp"

#include <array>

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include <core/common/endian.hpp>

#include <infra/common/log.hpp>
#include <infra/network/errors.hpp>

namespace zenpp::net {

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
    std::ignore = socket.open(time_server_adress.is_v6() ? udp::v6() : udp::v4(), error_code);
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

    udp::endpoint sender_endpoint;
    size_t len = socket.receive_from(buffer(recv_buf), sender_endpoint, 0, error_code);
    if (error_code) {
        return outcome::failure(error_code);
    }
    if (len != 48) {
        return Error::kInvalidNtpResponse;
    }

    // Interpret the ntp packet
    uint32_t transmitted_time = endian::load_big_u32(&recv_buf[40]);
    transmitted_time -= 2208988800U;  // NTP time starts in 1900, UNIX in 1970
    const auto transmitted_time_t = static_cast<std::time_t>(transmitted_time);
    const auto* transmitted_time_tm = std::gmtime(&transmitted_time_t);

    std::time_t system_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto* system_time_tm = std::gmtime(&system_time_t);

    std::ignore = log::Info(
        "Time Sync", {time_server, boost::replace_all_copy(std::string(std::asctime(transmitted_time_tm)), "\n", ""),
                      "system time", boost::replace_all_copy(std::string(std::asctime(system_time_tm)), "\n", "")});

    const auto delta_time = std::abs(system_time_t - transmitted_time_t);
    if (delta_time > max_skew_seconds) {
        return Error::kInvalidSystemTime;
    }

    return outcome::success();
}
}  // namespace zenpp::net
