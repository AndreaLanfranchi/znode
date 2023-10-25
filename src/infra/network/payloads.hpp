/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <optional>

#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

#include <infra/network/addresses.hpp>
#include <infra/network/errors.hpp>
#include <infra/network/protocol.hpp>

namespace zenpp::net {

//! \brief This class represents the payload of a NetMessage.
//! \details Is basically an abstract placeholder type to make semantically evident in function signatures
//! that a NetMessage payload is expected.
class MessagePayload : public ser::Serializable {
  public:
    explicit MessagePayload(MessageType message_type) : message_type_{message_type} {}
    ~MessagePayload() override = default;

    [[nodiscard]] MessageType type() const { return message_type_; }

  private:
    MessageType message_type_{MessageType::kMissingOrUnknown};
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override = 0;
};

class MsgNullPayload : public MessagePayload {
  public:
    explicit MsgNullPayload(MessageType message_type) : MessagePayload(message_type) {}
    ~MsgNullPayload() override = default;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& /*stream*/, ser::Action /*action*/) override {
        // Nothing to (de)serialize here
        return outcome::success();
    };
};

class MsgVersionPayload : public MessagePayload {
  public:
    MsgVersionPayload() : MessagePayload(MessageType::kVersion) {}
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
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgPingPayload : public MessagePayload {
  public:
    MsgPingPayload() : MessagePayload(MessageType::kPing) {}
    ~MsgPingPayload() override = default;

    uint64_t nonce_{0};

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgPongPayload : public MessagePayload {
  public:
    MsgPongPayload() : MessagePayload(MessageType::kPing) {}
    explicit MsgPongPayload(const MsgPingPayload& ping) : MessagePayload(MessageType::kPong), nonce_{ping.nonce_} {}
    ~MsgPongPayload() override = default;

    uint64_t nonce_{0};

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgGetHeadersPayload : public MessagePayload {
  public:
    MsgGetHeadersPayload() : MessagePayload(MessageType::kGetHeaders) {}
    ~MsgGetHeadersPayload() override = default;

    uint32_t protocol_version_{0};
    std::vector<h256> block_locator_hashes_{};
    h256 hash_stop_{};

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgAddrPayload : public MessagePayload {
  public:
    MsgAddrPayload() : MessagePayload(MessageType::kAddr) {}
    ~MsgAddrPayload() override = default;

    std::vector<NodeService> identifiers_{};

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgRejectPayload : public MessagePayload {
  public:
    MsgRejectPayload() : MessagePayload(MessageType::kReject) {}
    ~MsgRejectPayload() override = default;

    std::string rejected_command_{};
    RejectionCode rejection_code_{RejectionCode::kOk};
    std::string reason_{};              // Human readable reason for rejection
    std::optional<h256> extra_data_{};  // Optional extra data provided by the peer

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

}  // namespace zenpp::net
