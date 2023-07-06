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

static constexpr size_t kMaxProtocolMessageLength{static_cast<size_t>(4_MiB)};  // Maximum length of a protocol message
static constexpr size_t kMessageHeaderLength{24};                               // Length of a protocol message header
static constexpr size_t kMaxInvItems{50'000};                                   // Maximum number of inventory items
static constexpr size_t kInvItemSize{36};  // Size of an inventory item (type + hash)

enum class MessageType : uint32_t {
    kVersion,
    kVerack,
    kInv,
    kMissingOrUnknown,  // This must be the last entry
};

struct MessageDefinition {
    const char* command{nullptr};                              // The command string
    MessageType message_type{MessageType::kMissingOrUnknown};  // The command id
    const std::optional<size_t> max_vector_items{};            // The maximum number of vector items in the payload
    const std::optional<size_t> vector_item_size{};            // The size of a vector item
    const std::optional<size_t> min_payload_length{};          // The min allowed payload length
    const std::optional<size_t> max_payload_length{};          // The max allowed payload length
};

inline constexpr MessageDefinition kMessageVersion{
    "version",              //
    MessageType::kVersion,  //
    std::nullopt,           //
    std::nullopt,           //
    size_t{46},             //
    size_t(1_KiB)           //
};

inline constexpr MessageDefinition kMessageVerack{
    "verack",              //
    MessageType::kVerack,  //
    std::nullopt,          //
    std::nullopt,          //
    std::nullopt,          //
    size_t{0}              //
};

inline constexpr MessageDefinition kMessageInv{
    "inv",              //
    MessageType::kInv,  //
    size_t{kMaxInvItems},
    size_t{kInvItemSize},
    size_t{1 + kInvItemSize},                                                                 //
    size_t{serialization::ser_compact_sizeof(kMaxInvItems) + (kMaxInvItems * kInvItemSize)},  //
};

inline constexpr MessageDefinition kMessageMissingOrUnknown{
    nullptr,                         //
    MessageType::kMissingOrUnknown,  //
    size_t{0},
    size_t{0},
    size_t{0},  //
    size_t{0},  //
};

//! \brief List of all supported messages
//! \attention This must be kept in same order as the MessageCommand enum
inline constexpr std::array<MessageDefinition, 4> kMessageDefinitions{
    kMessageVersion,           // 0
    kMessageVerack,            // 1
    kMessageInv,               // 2
    kMessageMissingOrUnknown,  // 3

};

static_assert(kMessageDefinitions.size() == static_cast<size_t>(MessageType::kMissingOrUnknown) + 1,
              "kMessageDefinitions must be kept in same order as the MessageCommand enum");

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    uint32_t magic{0};                   // Message magic (origin network)
    std::array<uint8_t, 12> command{0};  // ASCII string identifying the packet content, NULL padded (non-NULL padding
                                         // results in message rejected)
    uint32_t length{0};                  // Length of payload in bytes
    std::array<uint8_t, 4> checksum{0};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    [[nodiscard]] MessageType get_type() const noexcept {
        return kMessageDefinitions[message_definition_id_].message_type;
    }
    [[nodiscard]] std::optional<size_t> max_vector_items() const noexcept {
        return kMessageDefinitions[message_definition_id_].max_vector_items;
    }
    [[nodiscard]] std::optional<size_t> min_payload_length() const noexcept {
        return kMessageDefinitions[message_definition_id_].min_payload_length;
    }
    [[nodiscard]] std::optional<size_t> max_payload_length() const noexcept {
        return kMessageDefinitions[message_definition_id_].max_payload_length;
    }
    void reset() noexcept;

    [[nodiscard]] serialization::Error validate(
        std::optional<uint32_t> expected_magic) const noexcept;  // Performs message validation

  private:
    mutable size_t message_definition_id_{static_cast<size_t>(MessageType::kMissingOrUnknown)};
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NetMessage {
  public:
    NetMessage() = delete;

    //! \brief Construct a NetMessage from a header and data
    //! \remarks This takes ownership of the header and data
    NetMessage(std::unique_ptr<NetMessageHeader>& header, std::unique_ptr<serialization::SDataStream>& data)
        : header_(std::move(header)), data_(std::move(data)) {}

    //! \brief Construct a NetMessage from a header and data
    //! \remarks This takes ownership of the header and data
    NetMessage(NetMessageHeader* header_ptr, serialization::SDataStream* data_ptr)
        : header_(header_ptr), data_(data_ptr) {}

    NetMessage(const NetMessage&) = delete;
    NetMessage(const NetMessage&&) = delete;
    NetMessage& operator=(const NetMessage&) = delete;
    ~NetMessage() = default;

    [[nodiscard]] NetMessageHeader& header() const noexcept { return *header_; }
    [[nodiscard]] serialization::SDataStream& data() const noexcept { return *data_; }

    //! \brief Validates the message header and payload
    [[nodiscard]] serialization::Error validate() const noexcept;

    [[nodiscard]] static serialization::Error validate_payload_checksum(ByteView payload,
                                                                        ByteView expected_checksum) noexcept;

  private:
    std::unique_ptr<NetMessageHeader> header_{nullptr};
    std::unique_ptr<serialization::SDataStream> data_{nullptr};
};

}  // namespace zen
