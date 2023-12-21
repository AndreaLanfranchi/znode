/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "addresses.hpp"

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <core/encoding/hex.hpp>

namespace znode::net {

TEST_CASE("IPAddress Parsing", "[infra][net][addresses]") {
    auto address{IPAddress::from_string("127.0.0.1")};
    REQUIRE(address.has_value());

    CHECK(address.value()->is_v4());
    CHECK(address.value().is_loopback());
    CHECK_FALSE(address.value().is_multicast());
    CHECK_FALSE(address.value().is_any());
    CHECK_FALSE(address.value().is_reserved());

    address = IPAddress::from_string("::1");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v6());
    CHECK(address.value().is_loopback());
    CHECK_FALSE(address.value().is_multicast());
    CHECK_FALSE(address.value().is_any());
    CHECK_FALSE(address.value().is_reserved());

    address = IPAddress::from_string("8.8.8.8");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v4());
    CHECK(!address.value().is_loopback());
    CHECK(!address.value().is_multicast());
    CHECK(!address.value().is_any());
    CHECK(!address.value().is_reserved());
    CHECK(address.value().get_type() == IPAddressType::kIPv4);

    address = IPAddress::from_string("2001::8888");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v6());
    CHECK(!address.value().is_loopback());
    CHECK(!address.value().is_multicast());
    CHECK(!address.value().is_any());
    CHECK(address.value().get_type() == IPAddressType::kIPv6);
    CHECK(address.value().address_reservation() == IPAddressReservationType::kRFC4380);

    address = IPAddress::from_string("2001::8888:9999");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v6());

    address = IPAddress::from_string("[2001::8888]:9999");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v6());

    address = IPAddress::from_string("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    REQUIRE(address.has_value());
    CHECK(address.value()->is_v6());

    address = IPAddress::from_string("2001::hgt:9999");
    REQUIRE_FALSE(address.has_value());
    REQUIRE(address.has_error());

    address = IPAddress::from_string("");
    REQUIRE(address.has_value());
    CHECK(address.value().is_unspecified());

    address = IPAddress::from_string("2001::8888:9999:9999");
    REQUIRE(address.has_value());
    CHECK_FALSE(address.value().is_unspecified());

    address = IPAddress::from_string("::FFFF:192.168.1.1");
    REQUIRE(address.has_value());
    CHECK_FALSE(address.value().is_unspecified());
    CHECK(address.value()->is_v4());
    CHECK(address.value().address_reservation() == IPAddressReservationType::kRFC1918);

    address = IPAddress::from_string("192.168.1.1:10");
    REQUIRE(address.has_value());
    CHECK_FALSE(address.value().is_unspecified());
    CHECK(address.value()->is_v4());
    CHECK(address.value().address_reservation() == IPAddressReservationType::kRFC1918);

    address = IPAddress::from_string("10.0.0.1:10");
    REQUIRE(address.has_value());
    CHECK_FALSE(address.value().is_unspecified());
    CHECK(address.value()->is_v4());
    CHECK(address.value().address_reservation() == IPAddressReservationType::kRFC1918);

    address = IPAddress::from_string("172.31.255.255");
    REQUIRE(address.has_value());
    CHECK_FALSE(address.value().is_unspecified());
    CHECK(address.value()->is_v4());
    CHECK(address.value().address_reservation() == IPAddressReservationType::kRFC1918);
    CHECK_FALSE(address.value().is_routable());
}

TEST_CASE("IPAddress Reservations", "[infra][net][addresses]") {
    using enum IPAddressReservationType;
    const std::vector<std::pair<std::string, IPAddressReservationType>> test_cases{
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
        INFO("Testing " << input);
        const auto parsed{IPAddress::from_string(input)};
        REQUIRE(parsed.has_value());
        CHECK_FALSE(parsed.value().is_unspecified());

        const auto& address{parsed.value()};
        std::string address_hexed{};
        if (address->is_v4()) {
            address_hexed = enc::hex::encode(address->to_v4().to_bytes());
        } else {
            address_hexed = enc::hex::encode(address->to_v6().to_bytes());
        }
        const auto expected_reservation{std::string(magic_enum::enum_name(reservation)) + " " + address_hexed};
        const auto actual_reservation{std::string(magic_enum::enum_name(address.address_reservation())) + " " +
                                      address_hexed};
        CHECK(expected_reservation == actual_reservation);
    }
}

TEST_CASE("IPSubNet parsing", "[infra][net][addresses]") {
    struct TestCase {
        std::string input;
        bool expected_valid;
        IPAddressType address_type;
        uint8_t prefix_length;
    };

    const std::vector<TestCase> test_cases{
        {"192.168.1.0/24", true, IPAddressType::kIPv4, 24},
        {"192.168.1.1/24", true, IPAddressType::kIPv4, 24},
        {"192.168.1.1/255.255.255.0", true, IPAddressType::kIPv4, 24},
        {"192.168.1.1/255.255.13.0", false, IPAddressType::kIPv4, 0},
        {"192.168.1.1/255.255.0.128", false, IPAddressType::kIPv4, 0},
        {"192.168.1.1/255.255.128.0", true, IPAddressType::kIPv4, 17},
        {"192.168.1.1/46", false, IPAddressType::kIPv4, 0},
        {"64:FF9B::/148", false, IPAddressType::kIPv6, 0},
        {"64:FF9B::/128", true, IPAddressType::kIPv6, 128},
    };

    for (const auto& [input, expected_valid, address_type, prefix_length] : test_cases) {
        INFO("Testing " << input);
        const auto parsed{IPSubNet::from_string(input)};
        if (expected_valid) {
            REQUIRE(parsed.has_value());
            const auto& subnet{parsed.value()};
            CHECK(subnet.is_valid());
            CHECK(subnet.base_address_.get_type() == address_type);
            CHECK(subnet.prefix_length_ == prefix_length);
        } else {
            REQUIRE(parsed.has_error());
        }
    }
}

TEST_CASE("IPSubNet contains", "[infra][net][addresses]") {
    struct TestCase {
        std::string subnet;
        std::string address;
        bool expected;
    };

    const std::vector<TestCase> test_cases{
        {"192.168.1.0/24", "192.168.1.10", true},
        {"192.168.1.0/24", "192.168.2.10", false},
        {"192.168.0.0/255.255.0.0", "192.168.1.10", true},
        {"192.168.0.0/255.255.0.0", "192.168.2.10", true},
        {"192.168.0.0/255.255.0.0", "192.169.2.10", false},
        {"10.0.0.0/8", "10.0.0.5", true},
        {"203.0.113.0/24", "203.0.113.50", true},
        {"2001:0db8:85a3::/48", "2001:0db8:85a3:0000:0000:8a2e:0370:7334", true},
        {"2001:0db8:85a3::/64", "2001:0db8:85a3:0000:0000:8a2e:0370:7334", true},
        {"2001:0db8:85a3:0000:0000:8a2e:0370:7000/80", "2001:0db8:85a3:0000:0010:8a2e:0370:7335", false},
    };

    for (const auto& test_case : test_cases) {
        INFO("Testing " << test_case.subnet << " contains " << test_case.address);
        const auto parsed_subnet{IPSubNet::from_string(test_case.subnet)};
        REQUIRE(parsed_subnet.has_value());
        const auto& subnet{parsed_subnet.value()};
        CHECK(subnet.is_valid());
        const auto parsed_address{IPAddress::from_string(test_case.address)};
        REQUIRE(parsed_address.has_value());
        const auto& address{parsed_address.value()};
        CHECK(address.is_valid());
        CHECK(subnet.contains(address) == test_case.expected);
    }
}

TEST_CASE("Network Endpoint Parsing", "[infra][net][addresses]") {
    struct TestCase {
        std::string input;
        boost::system::errc::errc_t expected_error;
        bool expected_valid;
        std::string expected_address;
        uint16_t expected_port;
    };

    /*
     * Given the IPv6 address "2001:0db8:0000:0000:0000:ff00:0042:8329"
     * For convenience, an IPv6 address may be compressed to reduce its length using these rules.
     * - One or more leading zeroes from any groups of hexadecimal digits are removed; this is usually done
     *   to either all or none of the leading zeroes. For example, the above IPv6 address can be abbreviated
     *   as: "2001:db8:0:0:0:ff00:42:8329".
     * - Consecutive sections of zeroes are replaced with a double colon (::). The double colon may only be
     *   used once in an address, as multiple use would render the address indeterminate.
     *   For example, the above IPv6 address can be abbreviated as: "2001:db8::ff00:42:8329".
     *   This form is sometimes known as "zero compression".
     * - The last two groups are written in IPv4 format. For example: "2001:db8:3333:4444:5555:6666:1.2.3.4".
     *   This form is compatible with a maximum compatibility option of dual-stack IPv6/IPv4 hosts.
     */

    const std::vector<TestCase> test_cases{
        {"8.8.8.4:8333", boost::system::errc::success, true, "8.8.8.4", 8333U},
        {"8.8.8.4:70000", boost::system::errc::value_too_large, false, "8.8.8.4", 0},
        {"8.8.8.4:xyz", boost::system::errc::invalid_argument, false, "8.8.8.4", 0},
        {"8.257.8.4:8333", boost::system::errc::bad_address, false, "", 0},
        {"::1:8333", boost::system::errc::success, false, "[::0.1.131.51]", 0},
        {"[::1]:8333", boost::system::errc::success, true, "[::1]", 8333U},
        {"[::1]", boost::system::errc::success, false, "[::1]", 0},
        {"not::valid", boost::system::errc::invalid_argument, false, "", 0},
        {"[::1]:80000", boost::system::errc::value_too_large, false, "", 0U},
    };

    for (const auto& test_case : test_cases) {
        const auto parsed{IPEndpoint::from_string(test_case.input)};
        if (test_case.expected_error not_eq boost::system::errc::success) {
            REQUIRE(parsed.has_error());
            CHECK(parsed.error().message() == boost::system::errc::make_error_code(test_case.expected_error).message());
        } else {
            REQUIRE_FALSE(parsed.has_error());
            REQUIRE(parsed.has_value());
            const auto& endpoint{parsed.value()};
            CHECK(endpoint.is_valid() == test_case.expected_valid);
            CHECK(endpoint.address_.to_string() == test_case.expected_address);
            CHECK(endpoint.port_ == test_case.expected_port);
        }
    }
}

TEST_CASE("Network Service Serialization", "[infra][net][addresses][serialization]") {
    const auto endpoint{IPEndpoint::from_string("10.0.0.1:8333")};
    REQUIRE(endpoint.has_value());

    NodeService service(endpoint.value());
    service.services_ = static_cast<decltype(NodeService::services_)>(NodeServicesType::kNodeNetwork);

    ser::SDataStream stream(ser::Scope::kNetwork, 0);
    CHECK(service.serialized_size(stream) == 30);
    stream.clear();
    REQUIRE_FALSE(service.serialize(stream).has_error());

    // See https://en.bitcoin.it/wiki/Protocol_documentation#Network_address
    const std::string expected_hex_dump(
        "00000000"
        "0100000000000000"
        "0000000000000000"
        "0000ffff0a000001"
        "208d");
    CHECK(stream.to_string() == expected_hex_dump);

    NodeService service2{};
    REQUIRE_FALSE(service2.deserialize(stream).has_error());
    CHECK(service2.services_ == service.services_);
    CHECK(service2.endpoint_ == service.endpoint_);
}

TEST_CASE("IPEndpoint SipHash Collision") {
    auto result{IPEndpoint::from_string("[2a02:c207:2054:4847::7]:9033")};
    REQUIRE(result.has_value());
    const auto ep1{result.value()};
    REQUIRE(ep1.port_ == 9033);
    REQUIRE(ep1.address_.get_type() == IPAddressType::kIPv6);

    result = IPEndpoint::from_string("209.126.0.125:9033");
    REQUIRE(result.has_value());
    const auto ep2{result.value()};
    REQUIRE(ep2.port_ == 9033);
    REQUIRE(ep2.address_.get_type() == IPAddressType::kIPv4);

    IPEndpointHasher hasher{};
    auto hash1{hasher(ep1)};
    auto hash2{hasher(ep2)};
    REQUIRE(hash1 != hash2);
}
}  // namespace znode::net
