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

NetAddress::NetAddress(std::string_view str) {
    if (str.empty()) return;
    uint16_t port_num{0};  // Only to ignore it
    std::ignore = try_parse_ip_address_and_port(str, value_, port_num);
    std::ignore = port_num;
}

NetAddress::NetAddress(boost::asio::ip::address address) : value_(std::move(address)) {}

bool NetAddress::is_loopback() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_loopback() : value_.to_v6().is_loopback();
}

bool NetAddress::is_multicast() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_multicast() : value_.to_v6().is_multicast();
}

bool NetAddress::is_any() const noexcept {
    return value_.is_v4() ? value_.to_v4() == boost::asio::ip::address_v4::any()
                          : value_.to_v6() == boost::asio::ip::address_v6::any();
}

bool NetAddress::is_unspecified() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_unspecified() : value_.to_v6().is_unspecified();
}

bool NetAddress::is_valid() const noexcept { return not(is_any() || is_unspecified()); }

bool NetAddress::is_routable() const noexcept {
    if (not is_valid() || is_loopback()) return false;

    switch (address_reservation()) {
        using enum AddressReservationType;
        case kRFC1918:
        case kRFC2544:
        case kRFC3927:
        case kRFC4862:
        case kRFC6598:
        case kRFC5737:
        case kRFC4193:
        case kRFC4843:
        case kRFC3849:
            return false;
        default:
            return true;
    }
}

bool NetAddress::is_reserved() const noexcept {
    using enum AddressReservationType;
    return address_reservation() not_eq kNotReserved;
}

AddressType NetAddress::get_type() const noexcept {
    if (not is_routable() or is_any()) return AddressType::kUnroutable;
    return value_.is_v4() ? AddressType::kIPv4 : AddressType::kIPv6;
}

AddressReservationType NetAddress::address_reservation() const noexcept {
    if (is_unspecified()) return AddressReservationType::kNotReserved;
    return value_.is_v4() ? address_v4_reservation() : address_v6_reservation();
}

AddressReservationType NetAddress::address_v4_reservation() const noexcept {
    using enum AddressReservationType;
    AddressReservationType ret{kNotReserved};
    if (!value_.is_v4()) return ret;

    const auto addr_bytes = value_.to_v4().to_bytes();

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

AddressReservationType NetAddress::address_v6_reservation() const noexcept {
    using enum AddressReservationType;
    AddressReservationType ret{kNotReserved};
    if (!value_.is_v6()) return ret;

    const auto addr_bytes = value_.to_v6().to_bytes();

    // Documentation Address Blocks
    if (addr_bytes[0] == 0x20 && addr_bytes[1] == 0x01 && addr_bytes[2] == 0x0D && addr_bytes[3] == 0xB8) {
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
    if (addr_bytes[0] == 0x00 && addr_bytes[1] == 0x64 && addr_bytes[2] == 0xFF && addr_bytes[3] == 0x9B) {
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

serialization::Error NetAddress::serialization(SDataStream& stream, serialization::Action action) {
    return stream.bind(value_, action);
}

NetEndpoint::NetEndpoint(std::string_view str) {
    if (str.empty()) return;
    boost::asio::ip::address address;
    if (try_parse_ip_address_and_port(str, address, port_)) {
        address_ = NetAddress{address};
    }
}

NetEndpoint::NetEndpoint(std::string_view str, uint16_t port_num) {
    if (str.empty()) return;
    boost::asio::ip::address address;
    std::ignore = try_parse_ip_address_and_port(str, address, port_);
    port_ = port_num;
}

NetEndpoint::NetEndpoint(boost::asio::ip::address address, uint16_t port_num)
    : address_{std::move(address)}, port_{port_num} {}

std::string NetEndpoint::to_string() const noexcept {
    std::string ret;
    if (address_->is_v6() and port_ not_eq 0) ret += '[';
    ret += address_->to_string();
    if (address_->is_v6() and port_ not_eq 0) ret += ']';
    if (port_ not_eq 0) ret += ':' + std::to_string(port_);
    return ret;
}

serialization::Error NetEndpoint::serialization(SDataStream& stream, serialization::Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (not ret) ret = stream.bind(address_, action);
    if (not ret) {
        port_ = bswap_16(port_);
        ret = stream.bind(port_, action);
        port_ = bswap_16(port_);
    }
    return ret;
}

boost::asio::ip::tcp::endpoint NetEndpoint::to_endpoint() const noexcept { return {*address_, port_}; }

NetEndpoint::NetEndpoint(const boost::asio::ip::tcp::endpoint& endpoint)
    : address_{endpoint.address()}, port_(endpoint.port()) {}

bool NetEndpoint::is_valid() const noexcept { return ((port_ > 1 and port_ < 65535) and address_.is_valid()); }

bool NetEndpoint::is_routable() const noexcept { return address_.is_routable() and (port_ > 1 and port_ < 65535); }

NetService::NetService(std::string_view str) : endpoint_{str} {}

NetService::NetService(std::string_view str, uint64_t services) : endpoint_{str}, services_{services} {}

NetService::NetService(std::string_view address, uint16_t port_num) : endpoint_(address, port_num) {}

NetService::NetService(boost::asio::ip::address address, uint16_t port_num) : endpoint_(std::move(address), port_num) {}

NetService::NetService(boost::asio::ip::tcp::endpoint& endpoint) : endpoint_(endpoint) {}

Error NetService::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (not ret) ret = stream.bind(time_, action);
    if (not ret) ret = stream.bind(services_, action);
    if (not ret) ret = stream.bind(endpoint_, action);
    return ret;
}

NetService::NetService(const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>& endpoint)
    : endpoint_{endpoint.address(), endpoint.port()} {}

Error VersionNetService::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (not ret) ret = stream.bind(services_, action);
    if (not ret) ret = stream.bind(endpoint_, action);
    return ret;
}
}  // namespace zenpp
