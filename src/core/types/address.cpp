/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <regex>
#include <utility>

#include <core/common/misc.hpp>
#include <core/types/address.hpp>

namespace zenpp {

using namespace serialization;

NodeContactInfo::NodeContactInfo(std::string_view endpoint) {
    if (endpoint.empty()) return;
    std::ignore = parse_ip_address_and_port(endpoint, ip_address_, port_number_);
}

NodeContactInfo::NodeContactInfo(std::string_view address, uint16_t port_num) {
    if (address.empty()) return;
    if (parse_ip_address_and_port(address, ip_address_, port_number_)) {
        port_number_ = port_num;
    }
}

NodeContactInfo::NodeContactInfo(boost::asio::ip::address address, uint16_t port_num)
    : ip_address_(std::move(address)), port_number_(port_num) {}

NodeContactInfo::NodeContactInfo(boost::asio::ip::tcp::endpoint& endpoint)
    : ip_address_(endpoint.address()), port_number_(endpoint.port()) {}

Error NodeContactInfo::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};

    if (!ret) ret = stream.bind(time_, action);
    if (!ret) ret = stream.bind(services_, action);
    // These two guys are big endian address is already BE by boost for port we need to swap bytes
    if (!ret) ret = stream.bind(ip_address_, action);
    port_number_ = bswap_16(port_number_);
    if (!ret) ret = stream.bind(port_number_, action);
    port_number_ = bswap_16(port_number_);

    return ret;
}

boost::asio::ip::tcp::endpoint NodeContactInfo::to_endpoint() const { return {ip_address_, port_number_}; }

bool NodeContactInfo::is_address_loopback() const { return ip_address_.is_loopback(); }

bool NodeContactInfo::is_address_multicast() const { return ip_address_.is_multicast(); }

bool NodeContactInfo::is_rfc3849() const {
    if (!ip_address_.is_v6()) return false;

    /* RFC3849 provides a prefix (2001:DB8::/32) for IPv6 documentation.
    So we need to check whether the first 32 bits (4 bytes) match 2001:0DB8 */
    const auto addr_bytes = ip_address_.to_v6().to_bytes();
    return addr_bytes[15] == 0x20 && addr_bytes[14] == 0x01 && addr_bytes[13] == 0x0D && addr_bytes[12] == 0xB8;
}
bool NodeContactInfo::is_rfc6145() const {
    /*
     * IPv4-Embedded IPv6 Address Format:
     * +--------------+--+-------------------+
       |   80 bits    |16|      32 bits      |
       +--------------+--+-------------------+
       | 0x00000000   |FF| IPv4 Address (32) |
       +--------------+--+-------------------+
     * */

    if (!ip_address_.is_v6()) return false;
    const auto addr_bytes = ip_address_.to_v6().to_bytes();
    for (int i{0}; i < 10; ++i) {
        if (addr_bytes[i] != 0x00) return false;
    }

    return addr_bytes[10] == 0xFF && addr_bytes[11] == 0xFF;
}
bool NodeContactInfo::is_rfc1918() const {
    /*
     * RFC1918 provides three prefixes for IPv4 private networks:
     */
    if (!ip_address_.is_v4()) return false;
    const auto addr_bytes = ip_address_.to_v4().to_bytes();
    return (addr_bytes[0] == 10) || (addr_bytes[0] == 172 && addr_bytes[1] >= 16 && addr_bytes[1] <= 31) ||
           (addr_bytes[0] == 192 && addr_bytes[1] == 168);
}

Error VersionNetworkAddress::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};

    if (!ret) ret = stream.bind(services_, action);
    // These two guys are big endian address is already BE by boost for port we need to swap bytes
    if (!ret) ret = stream.bind(ip_address_, action);
    port_number_ = bswap_16(port_number_);
    if (!ret) ret = stream.bind(port_number_, action);
    port_number_ = bswap_16(port_number_);

    return ret;
}

}  // namespace zenpp
