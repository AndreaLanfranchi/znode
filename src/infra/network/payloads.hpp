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
#include <cstdint>
#include <optional>

#include <nlohmann/json.hpp>

#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>
#include <core/types/inventory.hpp>

#include <infra/network/addresses.hpp>
#include <infra/network/errors.hpp>
#include <infra/network/protocol.hpp>

namespace znode::net {

//! \brief This class represents the payload of a NetMessage.
//! \details Is basically an abstract placeholder type to make semantically evident in function signatures
//! that a NetMessage payload is expected.
class MessagePayload : public ser::Serializable {
  public:
    explicit MessagePayload(MessageType message_type) : message_type_{message_type} {}
    ~MessagePayload() override = default;

    [[nodiscard]] MessageType type() const { return message_type_; }
    [[nodiscard]] virtual nlohmann::json to_json() const = 0;

    [[nodiscard]] static std::shared_ptr<MessagePayload> from_type(MessageType type);

  private:
    MessageType message_type_{MessageType::kMissingOrUnknown};
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override = 0;
};

class MsgNullPayload : public MessagePayload {
  public:
    explicit MsgNullPayload(MessageType message_type) : MessagePayload(message_type) {
        ASSERT_PRE(message_type == MessageType::kVerAck or message_type == MessageType::kMemPool or
                   message_type == MessageType::kGetAddr);
    }
    ~MsgNullPayload() override = default;

    [[nodiscard]] nlohmann::json to_json() const override { return {nlohmann::json::value_t::object}; }

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

    [[nodiscard]] nlohmann::json to_json() const override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgPingPongPayload : public MessagePayload {
  public:
    explicit MsgPingPongPayload(MessageType message_type, uint64_t nonce = 0U)
        : MessagePayload(message_type), nonce_{nonce} {
        ASSERT_PRE(message_type == MessageType::kPing or message_type == MessageType::kPong);
    }
    ~MsgPingPongPayload() override = default;

    uint64_t nonce_{0};

    [[nodiscard]] nlohmann::json to_json() const override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgGetHeadersPayload : public MessagePayload {
  public:
    MsgGetHeadersPayload() : MessagePayload(MessageType::kGetHeaders) {
        block_locator_hashes_.reserve(kMaxGetHeadersItems);
    }
    ~MsgGetHeadersPayload() override = default;

    uint32_t protocol_version_{0};
    std::vector<h256> block_locator_hashes_{};
    h256 hash_stop_{};

    [[nodiscard]] nlohmann::json to_json() const override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgAddrPayload : public MessagePayload {
  public:
    MsgAddrPayload() : MessagePayload(MessageType::kAddr) {}
    ~MsgAddrPayload() override = default;

    std::vector<NodeService> identifiers_{};

    void shuffle() noexcept;

    [[nodiscard]] nlohmann::json to_json() const override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

class MsgInventoryPayload : public MessagePayload {
  public:
    explicit MsgInventoryPayload(MessageType message_type) : MessagePayload(message_type) {
        ASSERT_PRE(message_type == MessageType::kInv or message_type == MessageType::kGetData or
                   message_type == MessageType::kNotFound);
        items_.reserve(kMaxInvItems);
    }
    ~MsgInventoryPayload() override = default;

    std::vector<InventoryItem> items_{};

    [[nodiscard]] nlohmann::json to_json() const override;

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
    std::string reason_{};              // Human-readable reason for rejection
    std::optional<h256> extra_data_{};  // Optional extra data provided by the peer

    [[nodiscard]] nlohmann::json to_json() const override;

  private:
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

}  // namespace znode::net
