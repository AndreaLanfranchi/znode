/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <core/serialization/serializable.hpp>

namespace zenpp {

enum class NetworkServicesType : uint32_t {
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

class NetworkAddress : public serialization::Serializable {
  public:
    NetworkAddress();
    NetworkAddress(const std::string& address_string, uint16_t port_num);
    explicit NetworkAddress(std::string endpoint_string);
    explicit NetworkAddress(boost::asio::ip::tcp::endpoint& endpoint);

    uint32_t time{0};                  // unix timestamp : not serialized if protocol version < 31402
    uint64_t services{0};              // services mask
    boost::asio::ip::address address;  // the actual network address
    uint16_t port{0};                  // Tcp port number

    [[nodiscard]] boost::asio::ip::tcp::endpoint to_endpoint() const;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

}  // namespace zenpp
