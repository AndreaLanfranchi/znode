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

    [[nodiscard]] boost::asio::ip::tcp::endpoint to_endpoint() const;

    [[nodiscard]] bool is_address_loopback() const;
    [[nodiscard]] bool is_address_multicast() const;

    [[nodiscard]] bool is_rfc1918() const;  // Address Allocation for Private Internets
    [[nodiscard]] bool is_rfc2544() const;  // Benchmarking Methodology for Network Interconnect Devices
    [[nodiscard]] bool is_rfc3927() const;  // Dynamic Configuration of IPv4 Link-Local Addresses
    [[nodiscard]] bool is_rfc3849() const;  // IPv6 Address Prefix Reserved for Documentation
    [[nodiscard]] bool is_rfc6145() const;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
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
