/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <functional>
#include <string_view>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <gsl/gsl_util>
#include <tl/expected.hpp>

#include <core/serialization/serializable.hpp>

namespace zenpp::net {

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

enum class IPConnectionType : uint8_t {
    kNone = 0U,            // Unspecified
    kInbound = 1U,         // Dial-in
    kOutbound = 2U,        // Dial-out
    kManualOutbound = 3U,  // Dial-out initiated by user via CLI or RPC call
    kSeedOutbound = 4U,    // Dial-out initiated by process to query seed nodes
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

    //! \brief Overrides boost's to_string methods so we always have IPv6 addresses enclosed in square brackets
    [[nodiscard]] std::string to_string() const noexcept;

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

    bool operator==(const IPEndpoint& other) const noexcept;

    IPAddress address_{};
    uint16_t port_{0};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class IPSubNet {
  public:
    IPSubNet() = default;
    //! \brief Parses a string representing an IP subnet
    //! \details The following formats are supported:
    //! - ipv4_address/prefix_length (CIDR notation)
    //! - ipv4_address/subnet_mask (dotted decimal notation)
    //! - ipv4_address (defaults to /32 CIDR notation)
    //! - ipv6_address/prefix_length (CIDR notation)
    //! - ipv6_address/subnet_mask (dotted decimal notation)
    //! - ipv6_address (defaults to /128)
    explicit IPSubNet(std::string_view value);

    ~IPSubNet() = default;

    [[nodiscard]] bool is_valid() const noexcept;

    //! \brief Returns whether the provided address is part of this subnet
    //! \remarks This method will return always false if the subnet is not valid
    //! \returns True if the address is part of this subnet, false otherwise
    [[nodiscard]] bool contains(const boost::asio::ip::address& address) const noexcept;
    [[nodiscard]] bool contains(const IPAddress& address) const noexcept;

    //! \brief Returns the string representation of this subnet
    //! \details The returned string will be in CIDR notation and IPv6 addresses will be enclosed in square brackets
    [[nodiscard]] std::string to_string() const noexcept;

    //! \brief Returns the prefix length of a given subnet mask
    //! \details The subnet mask must be a valid dotted decimal notation (for IPv4) or CIDR notation (for IPv4 / IPv6)
    //! \returns An unsigned integer on success or an error string on failure
    [[nodiscard]] static tl::expected<unsigned, std::string> parse_prefix_length(const std::string& value) noexcept;

    //! \brief Calculates the base subnet address from a given address and prefix length
    //! \returns An IPAddress on success or an error string on failure
    [[nodiscard]] static tl::expected<boost::asio::ip::address, std::string> calculate_subnet_base_address(
        const boost::asio::ip::address& address, unsigned prefix_length) noexcept;

    IPAddress base_address_{};
    uint8_t prefix_length_{0};

  private:
};

class IPConnection {
  public:
    IPConnection() = default;
    ~IPConnection() = default;

    IPConnection(const IPEndpoint& endpoint, IPConnectionType type) noexcept : endpoint_{endpoint}, type_{type} {
        ASSERT(type_ not_eq IPConnectionType::kNone);
    };

    bool operator==(const IPConnection& other) const noexcept = default;

    IPEndpoint endpoint_{};
    IPConnectionType type_{IPConnectionType::kNone};
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

//! \brief VersionNodeService subclasses NodeService only to customize serialization
//! in Version message where it is required to be serialized/deserialized **without** the time field.
class VersionNodeService : public NodeService {
  public:
    using NodeService::NodeService;
    ~VersionNodeService() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp::net

namespace std {

template <>
struct hash<zenpp::net::IPAddress> {
    size_t operator()(const zenpp::net::IPAddress& address) const noexcept {
        return hash<boost::asio::ip::address>()(*address);
    }
};

template <>
struct hash<zenpp::net::IPEndpoint> {
    size_t operator()(const zenpp::net::IPEndpoint& endpoint) const noexcept {
        return hash<zenpp::net::IPAddress>()(endpoint.address_) ^ hash<uint16_t>()(endpoint.port_);
    }
};

template <>
struct hash<zenpp::net::IPConnection> {
    size_t operator()(const zenpp::net::IPConnection& connection) const noexcept {
        return hash<zenpp::net::IPEndpoint>()(connection.endpoint_) ^
               hash<uint16_t>()(static_cast<uint16_t>(connection.type_));
    }
};

}  // namespace std
