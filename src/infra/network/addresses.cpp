/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "addresses.hpp"

#include <bit>
#include <regex>

#include <absl/strings/str_cat.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <core/common/misc.hpp>

namespace zenpp::net {

using namespace ser;

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

bool IPAddress::is_valid() const noexcept { return not(is_any() or is_unspecified()); }

bool IPAddress::is_routable() const noexcept {
    if (not is_valid() or is_loopback()) return false;

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
    if ((addr_bytes[0] == 10) or (addr_bytes[0] == 172 and addr_bytes[1] >= 16 and addr_bytes[1] <= 31) or
        (addr_bytes[0] == 192 and addr_bytes[1] == 168)) {
        ret = kRFC1918;
    }

    // Inter-network communications
    if (addr_bytes[0] == 192 and (addr_bytes[1] == 18 or addr_bytes[1] == 19)) {
        ret = kRFC2544;
    }

    // Shared Address Space
    if (addr_bytes[0] == 100 and (addr_bytes[1] >= 64 and addr_bytes[1] <= 127)) {
        ret = kRFC6598;
    }

    // Documentation Address Blocks
    if ((addr_bytes[0] == 192 and addr_bytes[1] == 0 and addr_bytes[2] == 2) or
        (addr_bytes[0] == 198 and addr_bytes[1] == 51 and addr_bytes[2] == 100) or
        (addr_bytes[0] == 203 and addr_bytes[1] == 0 and addr_bytes[2] == 113)) {
        ret = kRFC5737;
    }

    // Dynamic Configuration of IPv4 Link-Local Addresses
    if (addr_bytes[0] == 169 and addr_bytes[1] == 254) {
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
    if (addr_bytes[0] == 0x20 and addr_bytes[1] == 0x01 and addr_bytes[2] == 0x0D and addr_bytes[3] == 0xB8) {
        ret = kRFC3849;
    }

    // IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    if (addr_bytes[0] == 0x20 and addr_bytes[1] == 0x02) {
        ret = kRFC3964;
    }

    // Unique Local IPv6 Unicast Addresses
    if (addr_bytes[0] == 0xFC or addr_bytes[0] == 0xFD) {
        ret = kRFC4193;
    }

    // Teredo IPv6 tunneling
    if (addr_bytes[0] == 0x20 and addr_bytes[1] == 0x01 and addr_bytes[2] == 0x00 and addr_bytes[3] == 0x00) {
        ret = kRFC4380;
    }

    // An IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    if (addr_bytes[0] == 0x20 and addr_bytes[1] == 0x01 and addr_bytes[2] == 0x00 and
        ((addr_bytes[3] & 0xF0) == 0x10)) {
        ret = kRFC4843;
    }

    // IPv6 Stateless Address Autoconfiguration
    if (addr_bytes[0] == 0xFE and addr_bytes[1] == 0x80) {
        ret = kRFC4862;
    }

    // IPv6 Addressing of IPv4/IPv6 Translators
    if (addr_bytes[0] == 0x00 and addr_bytes[1] == 0x64 and addr_bytes[2] == 0xFF and addr_bytes[3] == 0x9B) {
        ret = kRFC6052;
    }

    // IP/ICMP Translation Algorithm
    if (addr_bytes[0] == 0x00 and addr_bytes[1] == 0x00 and addr_bytes[2] == 0xFF and addr_bytes[3] == 0xFF and
        addr_bytes[4] == 0x00 and addr_bytes[5] == 0x00 and addr_bytes[6] == 0x00 and addr_bytes[7] == 0x00 and
        addr_bytes[8] == 0x00 and addr_bytes[9] == 0x00 and addr_bytes[10] == 0x00 and addr_bytes[11] == 0x00 and
        addr_bytes[12] == 0x00 and addr_bytes[13] == 0x00 and addr_bytes[14] == 0x00 and addr_bytes[15] == 0x00) {
        ret = kRFC6145;
    }

    return ret;
}

outcome::result<void> IPAddress::serialization(SDataStream& stream, ser::Action action) {
    return stream.bind(value_, action);
}

outcome::result<IPAddress> IPAddress::from_string(const std::string& input) {
    if (input.empty()) return IPAddress();
    // Try to parse as IPv4
    boost::system::error_code error;
    boost::asio::ip::address_v4 address_v4{boost::asio::ip::make_address_v4(input, error)};
    if (not error) return IPAddress{address_v4};
    // Try to parse as IPv6
    boost::asio::ip::address_v6 address_v6{boost::asio::ip::make_address_v6(input, error)};
    if (error) return boost::system::errc::bad_address;
    if (address_v6.is_v4_mapped()) return IPAddress{address_v6.to_v4()};
    return IPAddress{address_v6};
}

std::string IPAddress::to_string() const noexcept {
    if (value_.is_v6()) return absl::StrCat("[", value_.to_v6().to_string(), "]");
    return value_.to_v4().to_string();
}

IPEndpoint::IPEndpoint(const boost::asio::ip::tcp::endpoint& endpoint)
    : address_{endpoint.address()}, port_(endpoint.port()) {}

IPEndpoint::IPEndpoint(const IPAddress& address) : address_{address} {}

IPEndpoint::IPEndpoint(const boost::asio::ip::address& address) : address_{address} {}

IPEndpoint::IPEndpoint(uint16_t port_num) : address_{}, port_{port_num} {}

IPEndpoint::IPEndpoint(const IPAddress& address, uint16_t port_num) : address_{address}, port_{port_num} {}

IPEndpoint::IPEndpoint(boost::asio::ip::address address, uint16_t port_num)
    : address_{std::move(address)}, port_{port_num} {}

outcome::result<IPEndpoint> IPEndpoint::from_string(const std::string& input) {
    if (input.empty()) return IPEndpoint();
    static const std::regex ipv4_pattern(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})(?::(\d+))?)");
    static const std::regex ipv6_pattern(R"(\[?([0-9a-f:]+)\]?(?::(\d+))?)", std::regex_constants::icase);
    static const std::regex ipv6_ipv4_pattern(R"(\[?::ffff:((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))\]?(?::(\d+))?)",
                                              std::regex_constants::icase);

    std::smatch matches;
    std::smatch::size_type address_match{0};
    std::smatch::size_type port_match{0};
    if (std::regex_match(input, matches, ipv6_ipv4_pattern)) {
        address_match = 1U;
        port_match = 4U;
    } else if (std::regex_match(input, matches, ipv4_pattern) or std::regex_match(input, matches, ipv6_pattern)) {
        address_match = 1U;
        port_match = 2U;
    } else {
        return boost::system::errc::invalid_argument;
    }

    auto parsed_address{IPAddress::from_string(matches[address_match].str())};
    if (parsed_address.has_error()) return parsed_address.error();
    if (matches[port_match].matched) {
        try {
            auto port_parsed{boost::lexical_cast<uint64_t>(matches[port_match].str())};
            if (port_parsed > 65535U) return boost::system::errc::value_too_large;
            return IPEndpoint{parsed_address.value(), boost::numeric_cast<uint16_t>(port_parsed)};
        } catch (const boost::bad_lexical_cast& /*exception*/) {
            return boost::system::errc::invalid_argument;
        }
    }
    return IPEndpoint{parsed_address.value()};
}

std::string IPEndpoint::to_string() const noexcept { return absl::StrCat(address_.to_string(), ":", port_); }

outcome::result<void> IPEndpoint::serialization(SDataStream& stream, ser::Action action) {
    auto result{stream.bind(address_, action)};
    if (not result.has_error()) result = stream.bind(address_, action);
    if (not result.has_error()) {
        port_ = bswap_16(port_);
        result = stream.bind(port_, action);
        port_ = bswap_16(port_);
    }
    return result;
}

boost::asio::ip::tcp::endpoint IPEndpoint::to_endpoint() const noexcept { return {*address_, port_}; }

bool IPEndpoint::is_valid() const noexcept { return ((port_ > 1 and port_ < 65535) and address_.is_valid()); }

bool IPEndpoint::is_routable() const noexcept { return address_.is_routable() and (port_ > 1 and port_ < 65535); }

bool IPEndpoint::operator==(const IPEndpoint& other) const noexcept {
    return address_ == other.address_ and port_ == other.port_;
}

bool IPSubNet::is_valid() const noexcept {
    return base_address_.is_valid() and
           (prefix_length_ > 0U and prefix_length_ <= (base_address_->is_v4() ? 32U : 128U));
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
    for (unsigned i{0}, end{static_cast<unsigned>(static_cast<unsigned>(prefix_length_) / unsigned(CHAR_BIT))}; i < end;
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

bool IPSubNet::contains(const IPAddress& address) const noexcept {
    if (not is_valid() or not address.is_valid()) return false;
    return contains(*address);
}

outcome::result<IPSubNet> IPSubNet::from_string(const std::string& input) {
    if (input.empty()) return IPSubNet();

    // Split the string into address and mask
    std::vector<std::string> parts;
    boost::split(parts, input, boost::is_any_of("/"));
    if (parts.size() > 2) return boost::system::errc::invalid_argument;

    // Try parse the address part
    auto parsed_address{IPAddress::from_string(parts[0])};
    if (not parsed_address) return parsed_address.error();

    // If we don't have a prefix length, we assume the maximum for the address type
    if (parts.size() == 1U) {
        return IPSubNet{parsed_address.value(),
                        gsl::narrow_cast<uint8_t>(parsed_address.value()->is_v4() ? 32U : 128U)};
    }

    // Try parse the prefix length
    auto parsed_prefix_length{parse_prefix_length(parts[1])};
    if (not parsed_prefix_length) return parsed_prefix_length.error();
    if (parsed_address.value()->is_v4() and parsed_prefix_length.value() > 32U) {
        return boost::system::errc::value_too_large;
    }

    return IPSubNet(parsed_address.value(), gsl::narrow_cast<uint8_t>(parsed_prefix_length.value()));
}

std::string IPSubNet::to_string() const noexcept {
    auto ret{absl::StrCat(base_address_->to_string(), "/", prefix_length_)};
    return ret;
}

outcome::result<uint8_t> IPSubNet::parse_prefix_length(const std::string& input) noexcept {
    if (input.empty()) return boost::system::errc::invalid_argument;

    unsigned ret{0};
    const std::regex decimal_notation_pattern(R"(^([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3})$)");
    const std::regex cidr_notation_pattern(R"(^([0-9]{1,3})$)");
    std::smatch matches;

    if (std::regex_match(input, matches, decimal_notation_pattern)) {
        bool zero_found{false};
        for (unsigned i{1}; i < 5U; ++i) {
            uint16_t octet_value{0};
            try {
                octet_value = boost::lexical_cast<uint16_t>(matches[i]);
            } catch (const boost::bad_lexical_cast&) {
                return boost::system::errc::invalid_argument;
            }
            switch (octet_value) {
                case 0U:
                    zero_found = true;
                    break;
                    /* valid octets >> */
                case 128U:
                case 192U:
                case 224U:
                case 240U:
                case 248U:
                case 252U:
                case 254U:
                case 255U:
                    if (zero_found) return boost::system::errc::illegal_byte_sequence;
                    ret += static_cast<decltype(ret)>(std::popcount(octet_value));
                    break;
                    /* valid octets << */
                default:
                    return boost::system::errc::invalid_argument;
            }
        }

    } else if (std::regex_match(input, matches, cidr_notation_pattern)) {
        try {
            ret = boost::lexical_cast<uint16_t>(input);
            if (ret > 128U) return boost::system::errc::value_too_large;
        } catch (const boost::bad_lexical_cast&) {
            return boost::system::errc::invalid_argument;
        }
    } else {
        // Not a recognized notation
        return boost::system::errc::invalid_argument;
    }
    return gsl::narrow_cast<uint8_t>(ret);
}

outcome::result<boost::asio::ip::address> IPSubNet::calculate_subnet_base_address(
    const boost::asio::ip::address& address, unsigned prefix_length) noexcept {
    if (address.is_v4()) {
        if (prefix_length > 32U) return boost::system::errc::value_too_large;
        const uint32_t mask = (0xFFFFFFFFU << (32U - prefix_length));
        const uint32_t address_int = address.to_v4().to_uint();
        const uint32_t subnet_int = mask & address_int;
        const std::array<unsigned char, 4> subnet_bytes{static_cast<unsigned char>((subnet_int >> 24) bitand 0xFFU),
                                                        static_cast<unsigned char>((subnet_int >> 16) bitand 0xFFU),
                                                        static_cast<unsigned char>((subnet_int >> 8) bitand 0xFFU),
                                                        static_cast<unsigned char>(subnet_int & 0xFFU)};

        return boost::asio::ip::make_address_v4(subnet_bytes);
    }

    if (prefix_length > 128U) return boost::system::errc::value_too_large;
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

outcome::result<IPAddress> IPSubNet::calculate_subnet_base_address(const IPAddress& address,
                                                                   unsigned prefix_length) noexcept {
    if (not address.is_valid()) return boost::system::errc::invalid_argument;
    auto ret{calculate_subnet_base_address(*address, prefix_length)};
    if (not ret) return ret.error();
    return IPAddress{ret.value()};
}

NodeService::NodeService(boost::asio::ip::address address, uint16_t port_num)
    : endpoint_(std::move(address), port_num) {}

NodeService::NodeService(boost::asio::ip::tcp::endpoint& endpoint) : endpoint_(endpoint) {}

NodeService::NodeService(const IPEndpoint& endpoint) : endpoint_(endpoint) {}

outcome::result<void> NodeService::serialization(SDataStream& stream, Action action) {
    auto result{stream.bind(time_, action)};
    if (not result.has_error()) result = stream.bind(time_, action);
    if (not result.has_error()) result = stream.bind(services_, action);
    if (not result.has_error()) result = stream.bind(endpoint_, action);
    return result;
}

NodeService::NodeService(const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>& endpoint)
    : endpoint_{endpoint.address(), endpoint.port()} {}

outcome::result<void> VersionNodeService::serialization(SDataStream& stream, Action action) {
    auto result{stream.bind(services_, action)};
    if (not result.has_error()) result = stream.bind(endpoint_, action);
    return result;
}
}  // namespace zenpp::net