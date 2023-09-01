/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <string_view>

#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <gsl/gsl_util>

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

enum class IPAddressReservationType {
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

enum class IPAddressType : uint8_t {
    kUnroutable = 0,
    kIPv4 = 1,
    kIPv6 = 2
};

class IPAddress : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    explicit IPAddress(std::string_view str);
    explicit IPAddress(boost::asio::ip::address address);
    ~IPAddress() override = default;

    boost::asio::ip::address operator*() const noexcept { return value_; };
    boost::asio::ip::address* operator->() noexcept { return &value_; };
    const boost::asio::ip::address* operator->() const noexcept {
        return const_cast<const boost::asio::ip::address*>(&value_);
    };

    [[nodiscard]] bool is_loopback() const noexcept;
    [[nodiscard]] bool is_multicast() const noexcept;
    [[nodiscard]] bool is_any() const noexcept;
    [[nodiscard]] bool is_unspecified() const noexcept;
    [[nodiscard]] bool is_reserved() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_routable() const noexcept;

    [[nodiscard]] IPAddressType get_type() const noexcept;
    [[nodiscard]] IPAddressReservationType address_reservation() const noexcept;

  private:
    boost::asio::ip::address value_{boost::asio::ip::address_v4()};
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
    [[nodiscard]] IPAddressReservationType address_v4_reservation() const noexcept;
    [[nodiscard]] IPAddressReservationType address_v6_reservation() const noexcept;
};

class IPEndpoint : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    explicit IPEndpoint(std::string_view str);
    explicit IPEndpoint(const boost::asio::ip::tcp::endpoint& endpoint);
    IPEndpoint(std::string_view str, uint16_t port_num);
    IPEndpoint(boost::asio::ip::address address, uint16_t port_num);
    ~IPEndpoint() override = default;

    [[nodiscard]] std::string to_string() const noexcept;
    [[nodiscard]] boost::asio::ip::tcp::endpoint to_endpoint() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_routable() const noexcept;

    IPAddress address_{};
    uint16_t port_{0};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NodeService : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    explicit NodeService(std::string_view str);
    explicit NodeService(boost::asio::ip::tcp::endpoint& endpoint);
    explicit NodeService(const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>& endpoint);
    NodeService(std::string_view str, uint64_t services);
    NodeService(std::string_view address, uint16_t port_num);
    NodeService(boost::asio::ip::address address, uint16_t port_num);
    ~NodeService() override = default;

    // Copy constructor
    NodeService(const NodeService& other) = default;

    uint32_t time_{0};       // unix timestamp
    uint64_t services_{0};   // services mask (OR'ed from NetworkServicesType)
    IPEndpoint endpoint_{};  // ipv4/ipv6 address and port

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

//! \brief VersionNetService subclasses NetService only to customize serialization
//! in Version message where it is required to be serialized/deserialized **without** the time field.
class VersionNetService : public NodeService {
  public:
    using NodeService::NodeService;
    ~VersionNetService() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp
