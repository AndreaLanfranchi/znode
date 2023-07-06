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
static constexpr size_t kInvItemSize{36};      // Size of an inventory item (type + hash)
static constexpr size_t kMaxAddrItems{1'000};  // Maximum number of items in an addr message
static constexpr size_t kAddrItemSize{30};     // Size of an address item (time + services + ip + port)

enum class NetMessageType : uint32_t {
    kVersion,           // Dial-out nodes send their version first
    kVerack,            // Reply by dial-in nodes to version message
    kInv,               // Inventory message to advertise the knowledge of hashes of blocks or transactions
    kAddr,              // Address message to advertise the knowledge of addresses of other nodes
    kMissingOrUnknown,  // This must be the last entry
};

struct MessageDefinition {
    const char* command{nullptr};                                    // The command string
    NetMessageType message_type{NetMessageType::kMissingOrUnknown};  // The command id
    const std::optional<size_t> max_vector_items{};    // The maximum number of vector items in the payload
    const std::optional<size_t> vector_item_size{};    // The size of a vector item
    const std::optional<size_t> min_payload_length{};  // The min allowed payload length
    const std::optional<size_t> max_payload_length{};  // The max allowed payload length
};

inline constexpr MessageDefinition kMessageVersion{
    "version",                 //
    NetMessageType::kVersion,  //
    std::nullopt,              //
    std::nullopt,              //
    size_t{46},                //
    size_t(1_KiB)              //
};

inline constexpr MessageDefinition kMessageVerack{
    "verack",                 //
    NetMessageType::kVerack,  //
    std::nullopt,             //
    std::nullopt,             //
    std::nullopt,             //
    size_t{0}                 //
};

inline constexpr MessageDefinition kMessageInv{
    "inv",                 //
    NetMessageType::kInv,  //
    size_t{kMaxInvItems},
    size_t{kInvItemSize},
    size_t{1 + kInvItemSize},                                                                 //
    size_t{serialization::ser_compact_sizeof(kMaxInvItems) + (kMaxInvItems * kInvItemSize)},  //
};

inline constexpr MessageDefinition kMessageAddr{
    "addr",                 //
    NetMessageType::kAddr,  //
    size_t{kMaxAddrItems},
    size_t{kAddrItemSize},
    size_t{1 + kAddrItemSize},                                                                   //
    size_t{serialization::ser_compact_sizeof(kMaxAddrItems) + (kMaxAddrItems * kAddrItemSize)},  //
};

inline constexpr MessageDefinition kMessageMissingOrUnknown{
    nullptr,                            //
    NetMessageType::kMissingOrUnknown,  //
    size_t{0},
    size_t{0},
    size_t{0},  //
    size_t{0},  //
};

//! \brief List of all supported messages
//! \attention This must be kept in same order as the MessageCommand enum
inline constexpr std::array<MessageDefinition, 5> kMessageDefinitions{
    kMessageVersion,           // 0
    kMessageVerack,            // 1
    kMessageInv,               // 2
    kMessageAddr,              // 3
    kMessageMissingOrUnknown,  // 4
};

static_assert(kMessageDefinitions.size() == static_cast<size_t>(NetMessageType::kMissingOrUnknown) + 1,
              "kMessageDefinitions must be kept in same order as the MessageCommand enum");

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    std::array<uint8_t, 4> magic{0};     // Message magic (origin network)
    std::array<uint8_t, 12> command{0};  // ASCII string identifying the packet content, NULL padded (non-NULL padding
                                         // results in message rejected)
    uint32_t length{0};                  // Length of payload in bytes
    std::array<uint8_t, 4> checksum{0};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    //! \brief Returns the decoded message type
    [[nodiscard]] NetMessageType get_type() const noexcept {
        return kMessageDefinitions[message_definition_id_].message_type;
    }

    //! \brief Sets the message type and fills the command field
    //! \remarks On non pristine headers, this function has no effect
    void set_type(NetMessageType type) noexcept;

    [[nodiscard]] std::optional<size_t> max_vector_items() const noexcept {
        return kMessageDefinitions[message_definition_id_].max_vector_items;
    }
    [[nodiscard]] std::optional<size_t> min_payload_length() const noexcept {
        return kMessageDefinitions[message_definition_id_].min_payload_length;
    }
    [[nodiscard]] std::optional<size_t> max_payload_length() const noexcept {
        return kMessageDefinitions[message_definition_id_].max_payload_length;
    }

    //! \brief Reset the header to its factory state
    void reset() noexcept;

    //! \brief Check if the header is in its factory state
    [[nodiscard]] bool pristine() const noexcept;

    //! \brief Performs a sanity check on the header
    [[nodiscard]] serialization::Error validate(
        std::optional<ByteView> expected_network_magic = std::nullopt) const noexcept;

  private:
    mutable size_t message_definition_id_{static_cast<size_t>(NetMessageType::kMissingOrUnknown)};
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NetMessage {
  public:
    //! \brief Construct a blank NetMessage
    NetMessage()
        : header_(std::make_unique<NetMessageHeader>()),
          raw_data_(std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, 0)){};

    //! \brief Construct a blank NetMessage with a specific version
    explicit NetMessage(int version)
        : header_(std::make_unique<NetMessageHeader>()),
          raw_data_(std::make_unique<serialization::SDataStream>(serialization::Scope::kNetwork, version)){};

    //! \brief Construct a NetMessage from a header and data
    //! \remarks This takes ownership of the header and data
    explicit NetMessage(std::unique_ptr<NetMessageHeader>& header, std::unique_ptr<serialization::SDataStream>& data)
        : header_(std::move(header)), raw_data_(std::move(data)) {}

    //! \brief Construct a NetMessage from a header and data
    //! \remarks This takes ownership of the header and data
    explicit NetMessage(NetMessageHeader* header_ptr, serialization::SDataStream* data_ptr)
        : header_(header_ptr), raw_data_(data_ptr) {}

    NetMessage(const NetMessage&) = delete;
    NetMessage(const NetMessage&&) = delete;
    NetMessage& operator=(const NetMessage&) = delete;
    ~NetMessage() = default;

    [[nodiscard]] NetMessageHeader& header() const noexcept { return *header_; }
    [[nodiscard]] serialization::SDataStream& data() const noexcept { return *raw_data_; }

    //! \brief Validates the message header and payload
    [[nodiscard]] serialization::Error validate() const noexcept;

    [[nodiscard]] static serialization::Error validate_payload_checksum(ByteView payload,
                                                                        ByteView expected_checksum) noexcept;

  private:
    std::unique_ptr<NetMessageHeader> header_{nullptr};
    std::unique_ptr<serialization::SDataStream> raw_data_{nullptr};
};

}  // namespace zen
