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

#pragma once

#include <functional>
#include <span>
#include <string_view>
#include <utility>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <gsl/gsl_util>
#include <nlohmann/json.hpp>

#include <core/common/time.hpp>
#include <core/crypto/evp_mac.hpp>
#include <core/serialization/serializable.hpp>

#include <infra/common/random.hpp>

namespace znode::net {

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
    kIPv4 = 1,
    kIPv6 = 4,
};

class IPAddress : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;
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

    //! \brief Parses a string representing an IP address
    static outcome::result<IPAddress> from_string(const std::string& input);

    //! \brief Overrides boost's to_string methods so we always have IPv6 addresses enclosed in square brackets
    [[nodiscard]] std::string to_string() const noexcept;

  private:
    boost::asio::ip::address value_{boost::asio::ip::address_v4()};
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
    [[nodiscard]] IPAddressReservationType address_v4_reservation() const noexcept;
    [[nodiscard]] IPAddressReservationType address_v6_reservation() const noexcept;
};

class IPEndpoint : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;
    explicit IPEndpoint(const boost::asio::ip::tcp::endpoint& endpoint);
    explicit IPEndpoint(const IPAddress& address);
    explicit IPEndpoint(const boost::asio::ip::address& address);
    explicit IPEndpoint(uint16_t port_num);
    IPEndpoint(const IPAddress& address, uint16_t port_num);
    IPEndpoint(boost::asio::ip::address address, uint16_t port_num);
    ~IPEndpoint() override = default;

    [[nodiscard]] boost::asio::ip::tcp::endpoint to_endpoint() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_routable() const noexcept;

    bool operator==(const IPEndpoint& other) const noexcept;

    IPAddress address_{};
    uint16_t port_{0};

    //! \brief Parses a string representing an IP endpoint
    static outcome::result<IPEndpoint> from_string(const std::string& input);

    //! \brief Overrides boost's to_string methods so we always have IPv6 addresses enclosed in square brackets
    [[nodiscard]] std::string to_string() const noexcept;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class IPEndpointHasher {
  public:
    IPEndpointHasher() = default;
    size_t operator()(const IPEndpoint& endpoint) const noexcept {
        crypto::SipHash24 hasher(seed_key_);
        hasher.update(endpoint.address_->to_v6().to_bytes());
        hasher.update(endpoint.port_);
        const auto result{hasher.finalize()};
        auto result_64{endian::load_big_u64(result.data())};
        return static_cast<size_t>(result_64);
    }

  private:
    const Bytes seed_key_{get_random_bytes(16 /* == 2 * sizeof(uint64_t) */)};
};

class IPSubNet {
  public:
    IPSubNet() = default;

    IPSubNet(const IPAddress& address, uint8_t prefix_length) noexcept
        : base_address_{address}, prefix_length_{prefix_length} {}

    IPSubNet(const boost::asio::ip::address& address, uint8_t prefix_length) noexcept
        : base_address_{address}, prefix_length_{prefix_length} {}

    ~IPSubNet() = default;

    [[nodiscard]] bool is_valid() const noexcept;

    //! \brief Returns whether the provided address is part of this subnet
    //! \remarks This method will return always false if the subnet is not valid
    //! \returns True if the address is part of this subnet, false otherwise
    [[nodiscard]] bool contains(const boost::asio::ip::address& address) const noexcept;

    //! \brief Returns whether the provided address is part of this subnet
    //! \remarks This method will return always false if the subnet is not valid
    //! \returns True if the address is part of this subnet, false otherwise
    [[nodiscard]] bool contains(const IPAddress& address) const noexcept;

    //! \brief Parses a string representing an IP subnet
    //! \details The following formats are supported:
    //! - ipv4_address/prefix_length (CIDR notation)
    //! - ipv4_address/subnet_mask (dotted decimal notation)
    //! - ipv4_address (defaults to /32 CIDR notation)
    //! - ipv6_address/prefix_length (CIDR notation)
    //! - ipv6_address/subnet_mask (dotted decimal notation)
    //! - ipv6_address (defaults to /128)
    static outcome::result<IPSubNet> from_string(const std::string& input);

    //! \brief Returns the string representation of this subnet
    //! \details The returned string will be in CIDR notation and IPv6 addresses will be enclosed in square brackets
    [[nodiscard]] std::string to_string() const noexcept;

    //! \brief Returns the prefix length of a given subnet mask
    //! \details The subnet mask must be a valid dotted decimal notation (for IPv4) or CIDR notation (for IPv4 / IPv6)
    [[nodiscard]] static outcome::result<uint8_t> parse_prefix_length(const std::string& input) noexcept;

    //! \brief Calculates the base subnet address from a given address and prefix length
    //! \returns An boost::asio::ip::address on success or an error on failure
    [[nodiscard]] static outcome::result<boost::asio::ip::address> calculate_subnet_base_address(
        const boost::asio::ip::address& address, unsigned prefix_length) noexcept;

    //! \brief Calculates the base subnet address from a given address and prefix length
    //! \returns An IPAddress on success or an error on failure
    [[nodiscard]] static outcome::result<IPAddress> calculate_subnet_base_address(const IPAddress& address,
                                                                                  unsigned prefix_length) noexcept;

    IPAddress base_address_{};
    uint8_t prefix_length_{0};

  private:
};

class NodeService : public ser::Serializable {
  private:
    static constexpr std::chrono::seconds kTimeInit{100'000'000};

  public:
    using ser::Serializable::Serializable;
    explicit NodeService(const boost::asio::ip::tcp::endpoint& endpoint);
    explicit NodeService(const IPEndpoint& endpoint);
    NodeService(boost::asio::ip::address address, uint16_t port_num);
    ~NodeService() override = default;

    // Copy constructor
    NodeService(const NodeService& other) = default;

    NodeSeconds time_{kTimeInit};  // unix timestamp 4 bytes
    uint64_t services_{0};         // services mask (OR'ed from NetworkServicesType) 8 bytes
    IPEndpoint endpoint_{};        // ipv4/ipv6 address and port 18 bytes

    [[nodiscard]] virtual nlohmann::json to_json() const noexcept;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class NodeServiceInfo : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;
    explicit NodeServiceInfo(const NodeService& node_service, const IPAddress& source)
        : service_{node_service}, origin_(source){};
    explicit NodeServiceInfo(NodeService&& node_service, const IPAddress& source)
        : service_{node_service}, origin_(source){};
    ~NodeServiceInfo() override = default;

    //! \brief How old an address can be before being forgotten
    static constexpr std::chrono::days kMaxDaysSinceLastSeen{30L};

    //! \brief After how many connection attempts a new peer is considered bad
    static constexpr uint32_t kNewPeerMaxRetries{3L};

    //! \brief How long a connection can be deemed recent
    static constexpr std::chrono::days kRecentConnectionDays{7L};

    //! \brief How many connection failures are allowed in the "recent" history of this
    static constexpr uint32_t kMaxReconnectionFailures{10L};

    NodeService service_{};                                         // The actual service this class is bound to
    IPAddress origin_{};                                            // The original address advertising this
    NodeSeconds last_connection_attempt_{std::chrono::seconds(0)};  // Last time a connection has been attempted
    NodeSeconds last_connection_success_{std::chrono::seconds(0)};  // Last time a connection has been successful
    uint32_t connection_attempts_{0};                               // Attempts count since last successful connection
    uint32_t random_pos_{0};            // Actual position in the randomly ordered ids vector (memory)
    bool in_tried_bucket_{false};       // Whether this entry is in any of the "tried" buckets (memory)
    uint32_t new_references_count_{0};  // Number of times this entry has been referenced in the "new" buckets (memory)

    //! \brief Returns whether this service statistics are bad and as a result can be forgotten
    [[nodiscard]] bool is_bad(NodeSeconds now = Now<NodeSeconds>()) const noexcept;

    //! \brief Returns the relative chance of this service to be selected for a connection attempt when selecting
    //! nodes for outbound connections
    [[nodiscard]] double get_chance(NodeSeconds now = Now<NodeSeconds>()) const noexcept;

    [[nodiscard]] nlohmann::json to_json() const noexcept;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

//! \brief VersionNodeService subclasses NodeService only to customize serialization
//! in Version message where it is required to be serialized/deserialized **without** the time field.
class VersionNodeService : public NodeService {
  public:
    using NodeService::NodeService;
    ~VersionNodeService() override = default;

    [[nodiscard]] nlohmann::json to_json() const noexcept override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};
}  // namespace znode::net
