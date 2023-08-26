/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <core/abi/netmessage.hpp>
#include <core/types/address.hpp>
#include <core/types/hash.hpp>

namespace zenpp::abi {

class NullData : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~NullData() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream&, serialization::Action) override {
        // Nothing to (de)serialize here
        return serialization::Error::kSuccess;
    };
};

class Version : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~Version() override = default;

    int32_t version{0};
    uint64_t services{static_cast<uint64_t>(NodeServicesType::kNone)};
    int64_t timestamp{0};
    VersionNetworkAddress addr_recv{};
    VersionNetworkAddress addr_from{};
    uint64_t nonce{0};
    std::string user_agent{};
    int32_t start_height{0};
    bool relay{false};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class PingPong : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~PingPong() override = default;

    uint64_t nonce{0};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class GetHeaders : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~GetHeaders() override = default;

    uint32_t version{0};
    std::vector<h256> block_locator_hashes{};
    h256 hash_stop{};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class Addr : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~Addr() override = default;

    std::vector<NodeContactInfo> addresses{};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

}  // namespace zenpp::abi
