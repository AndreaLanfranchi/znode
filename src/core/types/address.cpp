/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/types/address.hpp>

namespace zenpp {

using namespace serialization;

NetworkAddress::NetworkAddress() { address = boost::asio::ip::address_v4(); }

NetworkAddress::NetworkAddress(std::string address_string, uint16_t port_num) {
    boost::system::error_code ec;
    address = boost::asio::ip::make_address(address_string, ec);
    if (ec) address = boost::asio::ip::address_v4();
    port = port_num;
}

Error NetworkAddress::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};

    if (!ret && stream.get_version() >= 31402) ret = stream.bind(time, action);
    if (!ret) ret = stream.bind(services, action);
    // These two guys are big endian
    // address is already BE by boost
    // for port we need to swap bytes
    if (!ret) ret = stream.bind(address, action);
    port = _byteswap_ushort(port);
    if (!ret) ret = stream.bind(port, action);
    port = _byteswap_ushort(port);

    return ret;
}

}  // namespace zenpp
