/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <regex>

#include <boost/algorithm/string/predicate.hpp>

#include <core/types/address.hpp>

namespace zenpp {

using namespace serialization;

NetworkAddress::NetworkAddress() { address = boost::asio::ip::address_v4(); }

NetworkAddress::NetworkAddress(std::string endpoint_string) {
    address = boost::asio::ip::address_v4();
    port = 0;

    if (endpoint_string.empty()) return;

    const std::regex pattern(R"(^([\d\.]*|\*|localhost)(:[\d]{1,4})?$)", std::regex_constants::icase);
    std::smatch matches;
    if (!std::regex_match(endpoint_string, matches, pattern)) return;

    std::string address_part{matches[1].str()};
    std::string port_part{matches[2].str()};
    if (address_part.empty() || boost::iequals(address_part, "*")) {
        address_part = "0.0.0.0";
    } else if (boost::iequals(address_part, "localhost")) {
        address_part = "127.0.0.1";
    }
    if (!port_part.empty()) {
        port_part.erase(0, 1);  // Get rid of initial ":"
    }
    // Validate IP address
    boost::system::error_code err;
    address = boost::asio::ip::make_address(address_part, err);
    if (err) return;
    // Validate port
    if (!port_part.empty()) {
        if (int p{std::stoi(port_part)}; p > 0 && p < 65535) {
            port = static_cast<uint16_t>(p);
        }
    }
}

NetworkAddress::NetworkAddress(const std::string& address_string, uint16_t port_num) {
    boost::system::error_code ec;
    address = boost::asio::ip::make_address(address_string, ec);
    if (ec) address = boost::asio::ip::address_v4();
    port = port_num;
}

NetworkAddress::NetworkAddress(boost::asio::ip::tcp::endpoint& endpoint)
    : address(endpoint.address()), port(endpoint.port()) {}

Error NetworkAddress::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};

    if (!ret) ret = stream.bind(time, action);
    if (!ret) ret = stream.bind(services, action);
    // These two guys are big endian address is already BE by boost for port we need to swap bytes
    if (!ret) ret = stream.bind(address, action);
    port = _byteswap_ushort(port);
    if (!ret) ret = stream.bind(port, action);
    port = _byteswap_ushort(port);

    return ret;
}

boost::asio::ip::tcp::endpoint NetworkAddress::to_endpoint() const { return {address, port}; }

Error VersionNetworkAddress::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};

    if (!ret) ret = stream.bind(services, action);
    // These two guys are big endian address is already BE by boost for port we need to swap bytes
    if (!ret) ret = stream.bind(address, action);
    port = _byteswap_ushort(port);
    if (!ret) ret = stream.bind(port, action);
    port = _byteswap_ushort(port);

    return ret;
}
}  // namespace zenpp
