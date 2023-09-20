/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

#include <infra/network/addresses.hpp>

namespace zenpp::net {

//! \brief This class represents the payload of a NetMessage.
//! \details Is basically an abstract placeholder type to make semantically evident in function signatures
//! that a NetMessage payload is expected.
class MessagePayload : public ser::Serializable {
  public:
    using ser::Serializable::Serializable;
    ~MessagePayload() override = default;

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& stream, ser::Action action) override = 0;
};

class MsgNullPayload : public MessagePayload {
  public:
    using MessagePayload::MessagePayload;
    ~MsgNullPayload() override = default;

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& /*stream*/, ser::Action /*action*/) override {
        // Nothing to (de)serialize here
        return ser::Error::kSuccess;
    };
};

class MsgVersionPayload : public MessagePayload {
  public:
    using MessagePayload::MessagePayload;
    ~MsgVersionPayload() override = default;

    int32_t protocol_version_{0};
    uint64_t services_{static_cast<uint64_t>(NodeServicesType::kNone)};
    int64_t timestamp_{0};
    VersionNodeService recipient_service_{};
    VersionNodeService sender_service_{};
    uint64_t nonce_{0};
    std::string user_agent_{};
    int32_t last_block_height_{0};
    bool relay_{false};

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgPingPongPayload : public MessagePayload {
  public:
    using MessagePayload::MessagePayload;
    ~MsgPingPongPayload() override = default;

    uint64_t nonce_{0};

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgGetHeadersPayload : public MessagePayload {
  public:
    using MessagePayload::MessagePayload;
    ~MsgGetHeadersPayload() override = default;

    uint32_t protocol_version_{0};
    std::vector<h256> block_locator_hashes_{};
    h256 hash_stop_{};

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgAddrPayload : public MessagePayload {
  public:
    using MessagePayload::MessagePayload;
    ~MsgAddrPayload() override = default;

    std::vector<NodeService> identifiers_{};

  private:
    friend class ser::SDataStream;
    ser::Error serialization(ser::SDataStream& stream, ser::Action action) override;
};
}  // namespace zenpp::net
