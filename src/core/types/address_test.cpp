/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/encoding/hex.hpp>
#include <core/types/address.hpp>

#include "magic_enum.hpp"

namespace zenpp {

TEST_CASE("Network address Parsing", "[types]") {
    NetAddress address("127.0.0.1");
    CHECK(address->is_v4());
    CHECK(address.is_loopback());
    CHECK(!address.is_multicast());
    CHECK(!address.is_any());
    CHECK(!address.is_reserved());
    CHECK(address.get_type() == AddressType::kIPv4);

    address = NetAddress("::1");
    CHECK(address->is_v6());
    CHECK(address.is_loopback());
    CHECK(!address.is_multicast());
    CHECK(!address.is_any());
    CHECK(!address.is_reserved());

    address = NetAddress("8.8.8.8");
    CHECK(address->is_v4());
    CHECK(!address.is_loopback());
    CHECK(!address.is_multicast());
    CHECK(!address.is_any());
    CHECK(!address.is_reserved());
    CHECK(address.get_type() == AddressType::kIPv6);

    address = NetAddress("2001::8888");
    CHECK(address->is_v6());
    CHECK(!address.is_loopback());
    CHECK(!address.is_multicast());
    CHECK(!address.is_any());
    CHECK(address.address_reservation() == AddressReservationType::kRFC4380);

    address = NetAddress("2001::8888:9999");
    CHECK(address->is_v6());

    address = NetAddress("[2001::8888]:9999");
    CHECK(address->is_v6());

    address = NetAddress("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    CHECK(address->is_v6());

    address = NetAddress("2001::hgt:9999");
    CHECK(address.is_unspecified());

    address = NetAddress("2001::8888:9999:9999");
    CHECK(!address.is_unspecified());

    address = NetAddress("::FFFF:192.168.1.1");
    CHECK(!address.is_unspecified());
    CHECK(address->is_v4());
    CHECK(address.address_reservation() == AddressReservationType::kRFC1918);

    address = NetAddress("192.168.1.1:10");
    CHECK(!address.is_unspecified());
    CHECK(address->is_v4());
    CHECK(address.address_reservation() == AddressReservationType::kRFC1918);

    address = NetAddress("10.0.0.1:10");
    CHECK(!address.is_unspecified());
    CHECK(address->is_v4());
    CHECK(address.address_reservation() == AddressReservationType::kRFC1918);

    address = NetAddress("172.31.255.255");
    CHECK(!address.is_unspecified());
    CHECK(address->is_v4());
    CHECK(address.address_reservation() == AddressReservationType::kRFC1918);
    CHECK(address.get_type() == AddressType::kUnroutable);
}

TEST_CASE("Network Address Reservations", "[types]") {
    using enum AddressReservationType;
    const std::vector<std::pair<std::string_view, AddressReservationType>> test_cases{
        {"192.168.1.1", kRFC1918},
        {"10.0.0.1", kRFC1918},
        {"10.0.2.5", kRFC1918},
        {"172.31.255.255", kRFC1918},
        {"2001:0DB8::", kRFC3849},
        {"169.254.1.1", kRFC3927},
        {"2002::1", kRFC3964},
        {"fc00::", kRFC4193},
        {"fd87:d87e:eb43:edb1:8e4:3588:e546:35ca", kRFC4193},
        {"2001::2", kRFC4380},
        {"2001:10::", kRFC4843},
        {"FE80::", kRFC4862},
        {"64:FF9B::", kRFC6052},
        {"192.18.0.0", kRFC2544},
        {"192.19.0.0", kRFC2544},
        {"100.64.0.0", kRFC6598},
        {"100.100.0.0", kRFC6598},
        {"192.0.2.0", kRFC5737},
        {"198.51.100.0", kRFC5737},
        {"203.0.113.0", kRFC5737},
        {"169.254.0.0", kRFC3927},
        {"::1", kNotReserved},
        {"127.0.0.1", kNotReserved},
        {"8.8.8.8", kNotReserved},
        {"162.159.200.123", kNotReserved},
    };

    for (const auto& [input, reservation] : test_cases) {
        const NetAddress address{input};
        CHECK(not address.is_unspecified());

        std::string address_hexed{};
        if (address->is_v4()) {
            address_hexed = hex::encode(address->to_v4().to_bytes());
        } else {
            address_hexed = hex::encode(address->to_v6().to_bytes());
        }
        const auto expected_reservation{std::string(magic_enum::enum_name(reservation)) + " " + address_hexed};
        const auto actual_reservation{std::string(magic_enum::enum_name(address.address_reservation())) + " " +
                                      address_hexed};
        CHECK(expected_reservation == actual_reservation);
    }
}

TEST_CASE("Network Endpoint Parsing", "[types]") {
    NetEndpoint endpoint("8.8.8.4:8333");
    CHECK(endpoint.address_->is_v4());
    CHECK(endpoint.address_->to_v4().to_string() == "8.8.8.4:8333");
    CHECK(endpoint.port_ == 8333);

    endpoint = NetEndpoint("::1:8333");
    CHECK(endpoint.address_->is_v6());
    CHECK(endpoint.port_ == 0);
    CHECK(endpoint.to_string() == "::1:8333");

    endpoint = NetEndpoint("[::1]:8333");
    CHECK(endpoint.address_->is_v6());
    CHECK(endpoint.port_ == 8333);
    CHECK(endpoint.to_string() == "[::1]:8333");
}

TEST_CASE("Network Service Serialization", "[serialization]") {
    NetService service("10.0.0.1:8333");
    service.services_ = static_cast<decltype(NetService::services_)>(NodeServicesType::kNodeNetwork);

    serialization::SDataStream stream(serialization::Scope::kNetwork, 0);
    CHECK(service.serialized_size(stream) == 30);
    stream.clear();
    REQUIRE(service.serialize(stream) == serialization::Error::kSuccess);

    // See https://en.bitcoin.it/wiki/Protocol_documentation#Network_address
    const std::string expected_hex_dump(
        "00000000"
        "0100000000000000"
        "0000000000000000"
        "0000ffff0a000001"
        "208d");
    CHECK(stream.to_string() == expected_hex_dump);

    NetService service2{};
    REQUIRE(service2.deserialize(stream) == serialization::Error::kSuccess);
    CHECK(service2.services_ == service.services_);
    CHECK(service2.endpoint_ == service.endpoint_);
}
}  // namespace zenpp
