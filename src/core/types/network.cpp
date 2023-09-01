/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <bit>
#include <regex>

#include <absl/strings/str_cat.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <core/common/misc.hpp>
#include <core/types/network.hpp>

namespace zenpp {

using namespace serialization;

IPAddress::IPAddress(std::string_view str) {
    if (str.empty()) return;
    uint16_t port_num{0};  // Only to ignore it
    std::ignore = try_parse_ip_address_and_port(str, value_, port_num);
    std::ignore = port_num;
}

IPAddress::IPAddress(boost::asio::ip::address address) : value_(std::move(address)) {}

bool IPAddress::is_loopback() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_loopback() : value_.to_v6().is_loopback();
}

bool IPAddress::is_multicast() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_multicast() : value_.to_v6().is_multicast();
}

bool IPAddress::is_any() const noexcept {
    return value_.is_v4() ? value_.to_v4() == boost::asio::ip::address_v4::any()
                          : value_.to_v6() == boost::asio::ip::address_v6::any();
}

bool IPAddress::is_unspecified() const noexcept {
    return value_.is_v4() ? value_.to_v4().is_unspecified() : value_.to_v6().is_unspecified();
}

bool IPAddress::is_valid() const noexcept { return not(is_any() || is_unspecified()); }

bool IPAddress::is_routable() const noexcept {
    if (not is_valid() || is_loopback()) return false;

    switch (address_reservation()) {
        using enum IPAddressReservationType;
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

bool IPAddress::is_reserved() const noexcept {
    using enum IPAddressReservationType;
    return address_reservation() not_eq kNotReserved;
}

IPAddressType IPAddress::get_type() const noexcept {
    if (not is_routable() or is_any()) return IPAddressType::kUnroutable;
    return value_.is_v4() ? IPAddressType::kIPv4 : IPAddressType::kIPv6;
}

IPAddressReservationType IPAddress::address_reservation() const noexcept {
    if (is_unspecified()) return IPAddressReservationType::kNotReserved;
    return value_.is_v4() ? address_v4_reservation() : address_v6_reservation();
}

IPAddressReservationType IPAddress::address_v4_reservation() const noexcept {
    using enum IPAddressReservationType;
    IPAddressReservationType ret{kNotReserved};
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

IPAddressReservationType IPAddress::address_v6_reservation() const noexcept {
    using enum IPAddressReservationType;
    IPAddressReservationType ret{kNotReserved};
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

serialization::Error IPAddress::serialization(SDataStream& stream, serialization::Action action) {
    return stream.bind(value_, action);
}

std::string IPAddress::to_string() const noexcept {
    if (value_.is_v6()) return absl::StrCat("[", value_.to_string(), "]");
    return value_.to_string();
}

IPEndpoint::IPEndpoint(std::string_view str) {
    if (str.empty()) return;
    boost::asio::ip::address address;
    if (try_parse_ip_address_and_port(str, address, port_)) {
        address_ = IPAddress{address};
    }
}

IPEndpoint::IPEndpoint(std::string_view str, uint16_t port_num) {
    if (str.empty()) return;
    boost::asio::ip::address address;
    std::ignore = try_parse_ip_address_and_port(str, address, port_);
    port_ = port_num;
}

IPEndpoint::IPEndpoint(boost::asio::ip::address address, uint16_t port_num)
    : address_{std::move(address)}, port_{port_num} {}

std::string IPEndpoint::to_string() const noexcept { return absl::StrCat(address_.to_string(), ":", port_); }

serialization::Error IPEndpoint::serialization(SDataStream& stream, serialization::Action action) {
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

boost::asio::ip::tcp::endpoint IPEndpoint::to_endpoint() const noexcept { return {*address_, port_}; }

IPEndpoint::IPEndpoint(const boost::asio::ip::tcp::endpoint& endpoint)
    : address_{endpoint.address()}, port_(endpoint.port()) {}

bool IPEndpoint::is_valid() const noexcept { return ((port_ > 1 and port_ < 65535) and address_.is_valid()); }

bool IPEndpoint::is_routable() const noexcept { return address_.is_routable() and (port_ > 1 and port_ < 65535); }

IPSubNet::IPSubNet(const std::string_view value) {
    if (value.empty()) return;

    // Split the string into address and mask
    std::vector<std::string> parts;
    boost::split(parts, value, boost::is_any_of("/"));

    const IPAddress tmp_address{parts[0]};
    if (not tmp_address.is_valid()) return;

    if (parts.size() == 1U) {
        // No netmask or CIDR notation provided
        prefix_length_ = tmp_address->is_v4() ? 32 : 128;
    } else if (auto parsed{parse_prefix_length(parts[1])}; parsed) {
        prefix_length_ = gsl::narrow_cast<uint8_t>(*parsed);
    } else {
        return;  // Is not valid
    }

    if (auto expected_address{calculate_subnet_base_address(*tmp_address, prefix_length_)}; expected_address) {
        base_address_ = IPAddress(*expected_address);
    }
}

bool IPSubNet::is_valid() const noexcept {
    return base_address_.is_valid() and
           (prefix_length_ > 0U and prefix_length_ < (base_address_->is_v4() ? 33U : 129U));
}

bool IPSubNet::contains(const boost::asio::ip::address& address) const noexcept {
    if (not is_valid() or (address.is_unspecified() or address.is_loopback())) return false;
    if (base_address_->is_v4()) {
        if (not address.is_v4()) return false;
        const uint32_t mask = (0xFFFFFFFFU << (32U - prefix_length_));
        const auto address_int{address.to_v4().to_uint()};
        const auto subnet_int{base_address_->to_v4().to_uint()};
        return (address_int & mask) == subnet_int;
    }

    if (not address.is_v6()) return false;
    std::array<uint8_t, 16U> mask{0};
    for (unsigned i{0}, end{static_cast<unsigned>(static_cast<int>(prefix_length_) / unsigned(CHAR_BIT))}; i < end;
         ++i) {
        mask[i] = 0xFFU;
    }
    if (prefix_length_ % unsigned(CHAR_BIT) not_eq 0) {
        mask[prefix_length_ / unsigned(CHAR_BIT)] =
            0xFFU & (0xFFU << (unsigned(CHAR_BIT) - prefix_length_ % unsigned(CHAR_BIT)));
    }

    const auto address_bytes{address.to_v6().to_bytes()};
    const auto subnet_bytes{base_address_->to_v6().to_bytes()};

    for (unsigned i{0}; i < 16U; ++i) {
        if ((address_bytes[i] bitand mask[i]) not_eq subnet_bytes[i]) {
            return false;
        }
    }

    return true;
}

bool IPSubNet::contains(const zenpp::IPAddress& address) const noexcept {
    if (not is_valid() or not address.is_valid()) return false;
    return contains(*address);
}

std::string IPSubNet::to_string() const noexcept {
    if (not is_valid()) return {"invalid"};
    auto ret{absl::StrCat(base_address_->to_string(), "/", prefix_length_)};
    return ret;
}

tl::expected<unsigned, std::string> IPSubNet::parse_prefix_length(const std::string& value) noexcept {
    unsigned ret{0};
    if (value.empty()) return ret;

    const std::regex decimal_notation_pattern(R"(^([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3})$)");
    const std::regex cidr_notation_pattern(R"(^([0-9]{1,3})$)");
    std::smatch matches;

    if (std::regex_match(value, matches, decimal_notation_pattern)) {
        bool zero_found{false};
        for (unsigned i{1}; i < 5U; ++i) {
            const auto match_value{std::stoul(matches[i])};
            switch (match_value) {
                case 0:
                    zero_found = true;
                    break;
                    /* valid octets >> */
                case 128:
                case 192:
                case 224:
                case 240:
                case 248:
                case 252:
                case 254:
                case 255:
                    if (zero_found) return tl::unexpected("invalid_network_mask");
                    ret += std::popcount(match_value);
                    break;
                    /* valid octets << */
                default:
                    return tl::unexpected("invalid_network_mask");
            }
        }
        return ret;
    }

    if (std::regex_match(value, matches, cidr_notation_pattern)) {
        ret = std::stoul(value);
        if (ret > 128U) return tl::unexpected("invalid_prefix_length");
        return ret;
    }

    // Not a recognized notation
    return tl::unexpected("invalid_network_mask");
}

tl::expected<boost::asio::ip::address, std::string> IPSubNet::calculate_subnet_base_address(
    const boost::asio::ip::address& address, unsigned prefix_length) noexcept {
    if (address.is_v4()) {
        if (prefix_length > 32U) return tl::unexpected("invalid_prefix_length");
        const uint32_t mask = (0xFFFFFFFFU << (32U - prefix_length));
        const uint32_t address_int = address.to_v4().to_uint();
        const uint32_t subnet_int = mask & address_int;
        const std::array<unsigned char, 4> subnet_bytes{static_cast<unsigned char>((subnet_int >> 24) & 0xFFU),
                                                        static_cast<unsigned char>((subnet_int >> 16) & 0xFFU),
                                                        static_cast<unsigned char>((subnet_int >> 8) & 0xFFU),
                                                        static_cast<unsigned char>(subnet_int & 0xFFU)};

        return boost::asio::ip::make_address_v4(subnet_bytes);
    }

    if (prefix_length > 128U) return tl::unexpected("invalid_prefix_length");
    std::array<uint8_t, 16U> mask{0U};
    for (unsigned i{}, end{static_cast<unsigned>(prefix_length / unsigned(CHAR_BIT))}; i < end; ++i) {
        mask[i] = 0xFFU;
    }
    if (prefix_length % unsigned(CHAR_BIT) not_eq 0) {
        mask[prefix_length / unsigned(CHAR_BIT)] =
            0xFFU bitand (0xFFU << (unsigned(CHAR_BIT) - prefix_length % unsigned(CHAR_BIT)));
    }
    auto ipv6_bytes = address.to_v6().to_bytes();
    for (unsigned i{0}; i < 16U; ++i) {
        ipv6_bytes[i] and_eq mask[i];
    }
    return boost::asio::ip::address_v6(ipv6_bytes);
}

NodeService::NodeService(std::string_view str) : endpoint_{str} {}

NodeService::NodeService(std::string_view str, uint64_t services) : services_{services}, endpoint_{str} {}

NodeService::NodeService(std::string_view address, uint16_t port_num) : endpoint_(address, port_num) {}

NodeService::NodeService(boost::asio::ip::address address, uint16_t port_num)
    : endpoint_(std::move(address), port_num) {}

NodeService::NodeService(boost::asio::ip::tcp::endpoint& endpoint) : endpoint_(endpoint) {}

Error NodeService::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (not ret) ret = stream.bind(time_, action);
    if (not ret) ret = stream.bind(services_, action);
    if (not ret) ret = stream.bind(endpoint_, action);
    return ret;
}

NodeService::NodeService(const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>& endpoint)
    : endpoint_{endpoint.address(), endpoint.port()} {}

Error VersionNetService::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (not ret) ret = stream.bind(services_, action);
    if (not ret) ret = stream.bind(endpoint_, action);
    return ret;
}
}  // namespace zenpp
