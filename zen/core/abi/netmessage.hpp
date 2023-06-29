/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <memory>
#include <optional>

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/serializable.hpp>
#include <zen/core/types/hash.hpp>

namespace zen {

static constexpr uint32_t kMaxProtocolMessageLength{
    static_cast<uint32_t>(4_MiB)};  // Maximum length of a protocol message

static constexpr uint32_t kMessageHeaderLength{24};  // Length of a protocol message header

enum class MessageType : uint32_t {
    kVersion,
    kVerack,
    kInv,
    kMissingOrUnknown,  // This must be the last entry
};

struct MessageDefinition {
    const char* command{nullptr};                              // The command string
    MessageType message_type{MessageType::kMissingOrUnknown};  // The command id
    const std::optional<size_t> max_payload_length{};          // The max allowed payload length
};

inline constexpr MessageDefinition kMessageVersion{"version", MessageType::kVersion, 1_KiB};
inline constexpr MessageDefinition kMessageVerack{"verack", MessageType::kVerack, size_t{0}};
inline constexpr MessageDefinition kMessageInv{
    "inv", MessageType::kInv, (serialization::kMaxSerializedCompactSize * h256::size()) + size_t{9 /* compact */}};

//! \brief List of all supported messages
//! \attention This must be kept in same order as the MessageCommand enum
inline constexpr std::array<MessageDefinition, 3> kMessageDefinitions{
    kMessageVersion,  // 0
    kMessageVerack,   // 1
    kMessageInv       // 2
};

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    uint32_t magic{0};                   // Message magic (origin network)
    std::array<uint8_t, 12> command{0};  // ASCII string identifying the packet content, NULL padded (non-NULL padding
                                         // results in packet rejected)
    uint32_t length{0};                  // Length of payload in bytes
    std::array<uint8_t, 4> checksum{0};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    [[nodiscard]] MessageType get_type() const noexcept { return message_type; }
    void reset() noexcept;

    [[nodiscard]] serialization::Error validate(
        std::optional<uint32_t> expected_magic) const noexcept;  // Performs message validation

  private:
    mutable MessageType message_type{MessageType::kMissingOrUnknown};  // The command id

    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NetMessage {
  public:
    NetMessage() = delete;
    NetMessage(std::unique_ptr<NetMessageHeader>& header, std::unique_ptr<serialization::SDataStream>& data)
        : header_(std::move(header)), data_(std::move(data)) {}  // Take ownership of the header and data

    [[nodiscard]] NetMessageHeader& header() { return *header_; }
    [[nodiscard]] serialization::SDataStream& data() { return *data_; }

  private:
    std::unique_ptr<NetMessageHeader> header_;
    std::unique_ptr<serialization::SDataStream> data_;
};

}  // namespace zen
