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

NodeIdentifier::NodeIdentifier(std::string_view endpoint) {
    if (endpoint.empty()) return;
    std::ignore = try_parse_ip_address_and_port(endpoint, ip_address_, port_number_);
}

NodeIdentifier::NodeIdentifier(std::string_view address, uint16_t port_num) {
    if (address.empty()) return;
    if (try_parse_ip_address_and_port(address, ip_address_, port_number_)) {
        port_number_ = port_num;
    }
}

NodeIdentifier::NodeIdentifier(boost::asio::ip::address address, uint16_t port_num)
    : ip_address_(std::move(address)), port_number_(port_num) {}

NodeIdentifier::NodeIdentifier(boost::asio::ip::tcp::endpoint& endpoint)
    : ip_address_(endpoint.address()), port_number_(endpoint.port()) {}

Error NodeIdentifier::serialization(SDataStream& stream, Action action) {
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

boost::asio::ip::tcp::endpoint NodeIdentifier::get_endpoint() const { return {ip_address_, port_number_}; }

bool NodeIdentifier::is_address_loopback() const {
    return ip_address_.is_v4() ? ip_address_.to_v4().is_loopback() : ip_address_.to_v6().is_loopback();
}

bool NodeIdentifier::is_address_multicast() const {
    return ip_address_.is_v4() ? ip_address_.to_v4().is_multicast() : ip_address_.to_v6().is_multicast();
}

bool NodeIdentifier::is_address_any() const {
    if (ip_address_.is_v4()) {
        return ip_address_.to_v4() == boost::asio::ip::address_v4::any();
    }
    return ip_address_.to_v6() == boost::asio::ip::address_v6::any();
}

bool NodeIdentifier::is_address_unspecified() const {
    if (ip_address_.is_v4()) {
        return ip_address_.to_v4().is_unspecified();
    }
    return ip_address_.to_v6().is_unspecified();
}

bool NodeIdentifier::is_address_valid() const { return !(is_address_any() || is_address_unspecified()); }

bool NodeIdentifier::is_address_reserved() const {
    using enum AddressReservationType;
    return address_reservation() != kNotReserved;
}

AddressReservationType NodeIdentifier::address_reservation() const {
    if (is_address_unspecified()) return AddressReservationType::kNotReserved;
    if (ip_address_.is_v4()) {
        return address_v4_reservation();
    }
    return address_v6_reservation();
}

AddressReservationType NodeIdentifier::address_v4_reservation() const {
    using enum AddressReservationType;
    AddressReservationType ret{kNotReserved};
    if (!ip_address_.is_v4()) return ret;

    const auto addr_bytes = ip_address_.to_v4().to_bytes();

    // Private networks
    if ((addr_bytes[0] == 10) || (addr_bytes[0] == 172 && addr_bytes[1] >= 16 && addr_bytes[1] <= 31) ||
        (addr_bytes[0] == 192 && addr_bytes[1] == 168)) {
        ret = kRFC1918;
    }

    // Inter-network communications
    if (addr_bytes[0] == 192 && (addr_bytes[1] == 18 || addr_bytes[1] == 19)) {
        ret = kRFC2544;
    }

    // Shared Address Space
    if (addr_bytes[0] == 100 && (addr_bytes[1] >= 64 && addr_bytes[1] <= 127)) {
        ret = kRFC6598;
    }

    // Documentation Address Blocks
    if ((addr_bytes[0] == 192 && addr_bytes[1] == 0 && addr_bytes[2] == 2) ||
        (addr_bytes[0] == 198 && addr_bytes[1] == 51 && addr_bytes[2] == 100) ||
        (addr_bytes[0] == 203 && addr_bytes[1] == 0 && addr_bytes[2] == 113)) {
        ret = kRFC5737;
    }

    // Dynamic Configuration of IPv4 Link-Local Addresses
    if (addr_bytes[0] == 169 && addr_bytes[1] == 254) {
        ret = kRFC3927;
    }

    return ret;
}

AddressReservationType NodeIdentifier::address_v6_reservation() const {
    using enum AddressReservationType;
    AddressReservationType ret{kNotReserved};
    if (!ip_address_.is_v6()) return ret;

    const auto addr_bytes = ip_address_.to_v6().to_bytes();

    // Documentation Address Blocks
    if (addr_bytes[15] == 0x20 && addr_bytes[14] == 0x01 && addr_bytes[13] == 0x0D && addr_bytes[12] == 0xB8) {
        ret = kRFC3849;
    }

    // IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    if (addr_bytes[0] == 0x20 && addr_bytes[1] == 0x02) {
        ret = kRFC3964;
    }

    // Unique Local IPv6 Unicast Addresses
    if (addr_bytes[0] == 0xFC || addr_bytes[0] == 0xFD) {
        ret = kRFC4193;
    }

    // Teredo IPv6 tunneling
    if (addr_bytes[0] == 0x20 && addr_bytes[1] == 0x01 && addr_bytes[2] == 0x00 && addr_bytes[3] == 0x00) {
        ret = kRFC4380;
    }

    // An IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    if (addr_bytes[0] == 0x20 && addr_bytes[1] == 0x01 && addr_bytes[2] == 0x00 && ((addr_bytes[3] & 0xF0) == 0x10)) {
        ret = kRFC4843;
    }

    // IPv6 Stateless Address Autoconfiguration
    if (addr_bytes[0] == 0xFE && addr_bytes[1] == 0x80) {
        ret = kRFC4862;
    }

    // IPv6 Addressing of IPv4/IPv6 Translators
    if (addr_bytes[0] == 0x64 && addr_bytes[1] == 0xFF && addr_bytes[2] == 0x9B && addr_bytes[3] == 0x00) {
        ret = kRFC6052;
    }

    // IP/ICMP Translation Algorithm
    if (addr_bytes[0] == 0x00 && addr_bytes[1] == 0x00 && addr_bytes[2] == 0xFF && addr_bytes[3] == 0xFF &&
        addr_bytes[4] == 0x00 && addr_bytes[5] == 0x00 && addr_bytes[6] == 0x00 && addr_bytes[7] == 0x00 &&
        addr_bytes[8] == 0x00 && addr_bytes[9] == 0x00 && addr_bytes[10] == 0x00 && addr_bytes[11] == 0x00 &&
        addr_bytes[12] == 0x00 && addr_bytes[13] == 0x00 && addr_bytes[14] == 0x00 && addr_bytes[15] == 0x00) {
        ret = kRFC6145;
    }

    return ret;
}

Error VersionNodeIdentifier::serialization(SDataStream& stream, Action action) {
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
