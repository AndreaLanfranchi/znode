/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <string_view>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <core/common/time.hpp>
#include <core/serialization/serializable.hpp>

namespace zenpp {

enum class NodeServicesType : uint64_t {
    kNone = 0,                      // No services
    kNodeNetwork = 1 << 0,          // NODE_NETWORK
    kNodeGetUTXO = 1 << 1,          // NODE_GETUTXO
    kNodeBloom = 1 << 2,            // NODE_BLOOM
    kNodeWitness = 1 << 3,          // NODE_WITNESS
    kNodeXthin = 1 << 4,            // NODE_XTHIN
    kNodeCompactFilters = 1 << 6,   // NODE_COMPACT_FILTERS
    kNodeNetworkLimited = 1 << 10,  // NODE_NETWORK_LIMITED
    kNodeNetworkAll = kNodeNetwork | kNodeGetUTXO | kNodeBloom | kNodeWitness | kNodeXthin | kNodeCompactFilters |
                      kNodeNetworkLimited,
};

enum class AddressReservationType {
    kNotReserved = 0,
    kRFC1918 = 1,   // IPV4 Reservation : Allocation for Private Internets
    kRFC2544 = 2,   // IPV4 Reservation : inter-network communications (192.18.0.0/15)
    kRFC6598 = 3,   // IPV4 Reservation : Shared Address Space
    kRFC5737 = 4,   // IPV4 Reservation : Documentation Address Blocks
    kRFC3927 = 5,   // IPV4 Reservation : Dynamic Configuration of IPv4 Link-Local Addresses
    kRFC3849 = 6,   // IPV6 Reservation : Documentation Address Blocks
    kRFC3964 = 7,   // IPV6 Reservation : IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    kRFC4193 = 8,   // IPV6 Reservation : Unique Local IPv6 Unicast Addresses
    kRFC4380 = 9,   // IPV6 Reservation : Teredo IPv6 tunneling
    kRFC4843 = 10,  // IPV6 Reservation : An IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers (ORCHID)
    kRFC4862 = 11,  // IPV6 Reservation : IPv6 Stateless Address Autoconfiguration
    kRFC6052 = 12,  // IPV6 Reservation : IPv6 Addressing of IPv4/IPv6 Translators
    kRFC6145 = 13,  // IPV6 Reservation : IP/ICMP Translation Algorithm
};

class NodeIdentifier : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    NodeIdentifier(std::string_view address, uint16_t port_num);
    NodeIdentifier(boost::asio::ip::address address, uint16_t port_num);
    explicit NodeIdentifier(std::string_view endpoint);
    explicit NodeIdentifier(boost::asio::ip::tcp::endpoint& endpoint);

    NodeIdentifier(const NodeIdentifier& other) = default;

    uint32_t time_{0};      // unix timestamp
    uint64_t services_{0};  // services mask (OR'ed from NetworkServicesType)
    boost::asio::ip::address ip_address_{boost::asio::ip::address_v4()};
    uint16_t port_number_{0};

    [[nodiscard]] boost::asio::ip::tcp::endpoint get_endpoint() const;

    [[nodiscard]] bool is_address_loopback() const;
    [[nodiscard]] bool is_address_multicast() const;
    [[nodiscard]] bool is_address_any() const;
    [[nodiscard]] bool is_address_unspecified() const;
    [[nodiscard]] bool is_address_reserved() const;
    [[nodiscard]] bool is_address_valid() const;

    [[nodiscard]] AddressReservationType address_reservation() const;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;

    [[nodiscard]] AddressReservationType address_v4_reservation() const;
    [[nodiscard]] AddressReservationType address_v6_reservation() const;
};

//! \brief VersionNodeIdentifier subclasses NodeIdentifier only to customize serialization
//! in Version message where it is required to be serialized/deserialized **without** the time field.
class VersionNodeIdentifier : public NodeIdentifier {
  public:
    using NodeIdentifier::NodeIdentifier;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp
