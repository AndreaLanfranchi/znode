/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/types/address.hpp>

namespace zenpp {

TEST_CASE("Node Identifier Parsing", "[serialization]") {
    NodeIdentifier identifier("127.0.0.1");
    CHECK(identifier.ip_address_.is_v4());
    CHECK(identifier.port_number_ == 0);
    CHECK(identifier.is_address_loopback());
    CHECK(!identifier.is_address_multicast());
    CHECK(!identifier.is_address_any());
    CHECK(!identifier.is_address_reserved());

    identifier = NodeIdentifier("::1");
    CHECK(identifier.ip_address_.is_v6());
    CHECK(identifier.port_number_ == 0);
    CHECK(identifier.is_address_loopback());
    CHECK(!identifier.is_address_multicast());
    CHECK(!identifier.is_address_any());
    CHECK(!identifier.is_address_reserved());

    identifier = NodeIdentifier("8.8.8.8");
    CHECK(identifier.ip_address_.is_v4());
    CHECK(identifier.port_number_ == 0);
    CHECK(!identifier.is_address_loopback());
    CHECK(!identifier.is_address_multicast());
    CHECK(!identifier.is_address_any());
    CHECK(!identifier.is_address_reserved());

    identifier = NodeIdentifier("2001::8888");
    CHECK(identifier.ip_address_.is_v6());
    CHECK(identifier.port_number_ == 0);
    CHECK(!identifier.is_address_loopback());
    CHECK(!identifier.is_address_multicast());
    CHECK(!identifier.is_address_any());
    CHECK(identifier.address_reservation() == AddressReservationType::kRFC4380);

    identifier = NodeIdentifier("2001::8888:9999");
    CHECK(identifier.ip_address_.is_v6());
    CHECK(identifier.port_number_ == 0);

    identifier = NodeIdentifier("[2001::8888]:9999");
    CHECK(identifier.ip_address_.is_v6());
    CHECK(identifier.port_number_ == 9999);

    identifier = NodeIdentifier("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    CHECK(identifier.ip_address_.is_v6());
    CHECK(identifier.port_number_ == 0);
    CHECK(identifier.address_reservation() == AddressReservationType::kNotReserved);

    identifier = NodeIdentifier("2001::hgt:9999");
    CHECK(identifier.is_address_unspecified());
    CHECK(identifier.port_number_ == 0);

}

TEST_CASE("Node Identifier Serialization", "[serialization]") {
    NodeIdentifier address("10.0.0.1:8333");
    address.services_ = static_cast<decltype(NodeIdentifier::services_)>(NodeServicesType::kNodeNetwork);

    serialization::SDataStream stream(serialization::Scope::kNetwork, 0);
    CHECK(address.serialized_size(stream) == 30);
    stream.clear();
    REQUIRE(address.serialize(stream) == serialization::Error::kSuccess);

    // See https://en.bitcoin.it/wiki/Protocol_documentation#Network_address
    const std::string expected_hex_dump(
        "00000000"
        "0100000000000000"
        "0000000000000000"
        "0000ffff0a000001"
        "208d");
    CHECK(stream.to_string() == expected_hex_dump);

    NodeIdentifier address2{};
    REQUIRE(address2.deserialize(stream) == serialization::Error::kSuccess);
    CHECK(address2.services_ == address.services_);
    CHECK(address2.ip_address_ == address.ip_address_);
    CHECK(address2.port_number_ == address.port_number_);
}
}  // namespace zenpp
