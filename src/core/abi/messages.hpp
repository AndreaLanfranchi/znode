/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <core/abi/message_defs.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>
#include <core/types/network.hpp>

namespace zenpp::abi {

//! \brief This class represents the payload of a NetMessage.
//! \details Is basically an abstract placeholder type to make semantically evident in function signatures
//! that a NetMessage payload is expected.
class NetMessagePayload : public serialization::Serializable {
  public:
    using serialization::Serializable::Serializable;
    ~NetMessagePayload() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override = 0;
};

class MsgNullPayload : public NetMessagePayload {
  public:
    using NetMessagePayload::NetMessagePayload;
    ~MsgNullPayload() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& /*stream*/,
                                       serialization::Action /*action*/) override {
        // Nothing to (de)serialize here
        return serialization::Error::kSuccess;
    };
};

class MsgVersionPayload : public NetMessagePayload {
  public:
    using NetMessagePayload::NetMessagePayload;
    ~MsgVersionPayload() override = default;

    int32_t protocol_version_{0};
    uint64_t services_{static_cast<uint64_t>(NodeServicesType::kNone)};
    int64_t timestamp_{0};
    VersionNodeService addr_recv_{};
    VersionNodeService addr_from_{};
    uint64_t nonce_{0};
    std::string user_agent_{};
    int32_t last_block_height_{0};
    bool relay_{false};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class MsgPingPongPayload : public NetMessagePayload {
  public:
    using NetMessagePayload::NetMessagePayload;
    ~MsgPingPongPayload() override = default;

    uint64_t nonce_{0};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class MsgGetHeadersPayload : public NetMessagePayload {
  public:
    using NetMessagePayload::NetMessagePayload;
    ~MsgGetHeadersPayload() override = default;

    uint32_t protocol_version_{0};
    std::vector<h256> block_locator_hashes_{};
    h256 hash_stop_{};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class MsgAddrPayload : public NetMessagePayload {
  public:
    using NetMessagePayload::NetMessagePayload;
    ~MsgAddrPayload() override = default;

    std::vector<NodeService> identifiers_{};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp::abi
