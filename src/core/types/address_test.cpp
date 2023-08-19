/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/types/address.hpp>

namespace zenpp {

TEST_CASE("Network Address Serialization", "[serialization]") {
    NetworkAddress address{};
    address.services = static_cast<decltype(NetworkAddress::services)>(NetworkServicesType::kNodeNetwork);
    address.address = boost::asio::ip::make_address("10.0.0.1");
    address.port = 8333;

    serialization::SDataStream stream(serialization::Scope::kNetwork, 0);
    CHECK(address.serialized_size(stream) == 30);
    stream.clear();
    REQUIRE(address.serialize(stream) == serialization::Error::kSuccess);

    // See https://en.bitcoin.it/wiki/Protocol_documentation#Network_address
    std::string expected_hex_dump(
        "00000000"
        "0100000000000000"
        "0000000000000000"
        "0000ffff0a000001"
        "208d");
    CHECK(stream.to_string() == expected_hex_dump);

    NetworkAddress address2{};
    REQUIRE(address2.deserialize(stream) == serialization::Error::kSuccess);
    CHECK(address2.services == address.services);
    CHECK(address2.address == address.address);
    CHECK(address2.port == address.port);
}
}  // namespace zenpp
