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

#include <core/abi/messages.hpp>
#include <core/common/base.hpp>
#include <core/crypto/hash256.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

namespace zenpp::abi {

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
    kPing,              // Ping message to measure the latency of a connection
    kPong,              // Pong message to reply to a ping message
    kGetheaders,        // Getheaders message to request/send a list of block headers
    kHeaders,           // Headers message to send a list of block
    kGetaddr,           // Getaddr message to request a list of known active peers
    kMemPool,           // MemPool message to request/send a list of transactions in the mempool
    kMissingOrUnknown,  // This must be the last entry
};

struct MessageDefinition {
    const char* command{nullptr};                                    // The command string
    NetMessageType message_type{NetMessageType::kMissingOrUnknown};  // The command id
    const bool is_vectorized{false};                                 // Whether the payload is a vector of items
    const std::optional<size_t> max_vector_items{};    // The maximum number of vector items in the payload
    const std::optional<size_t> vector_item_size{};    // The size of a vector item
    const std::optional<size_t> min_payload_length{};  // The min allowed payload length
    const std::optional<size_t> max_payload_length{};  // The max allowed payload length
    const std::optional<int> min_protocol_version{};   // The min protocol version that supports this message
    const std::optional<int> max_protocol_version{};   // The max protocol version that supports this message
};

inline constexpr MessageDefinition kMessageVersion{
    "version",                 //
    NetMessageType::kVersion,  //
    false,
    std::nullopt,  //
    std::nullopt,  //
    size_t{46},    //
    size_t(1_KiB)  //
};

inline constexpr MessageDefinition kMessageVerack{
    "verack",                 //
    NetMessageType::kVerack,  //
    false,
    std::nullopt,  //
    std::nullopt,  //
    std::nullopt,  //
    size_t{0}      //
};

inline constexpr MessageDefinition kMessageInv{
    "inv",                 //
    NetMessageType::kInv,  //
    true,
    size_t{kMaxInvItems},
    size_t{kInvItemSize},
    size_t{1 + kInvItemSize},                                                                 //
    size_t{serialization::ser_compact_sizeof(kMaxInvItems) + (kMaxInvItems * kInvItemSize)},  //
};

inline constexpr MessageDefinition kMessageAddr{
    "addr",                 //
    NetMessageType::kAddr,  //
    true,
    size_t{kMaxAddrItems},
    size_t{kAddrItemSize},
    size_t{1 + kAddrItemSize},                                                                   //
    size_t{serialization::ser_compact_sizeof(kMaxAddrItems) + (kMaxAddrItems * kAddrItemSize)},  //
};

inline constexpr MessageDefinition kMessagePing{
    "ping",                 //
    NetMessageType::kPing,  //
    false,
    size_t{0},
    size_t{0},
    size_t{sizeof(uint64_t)},  //
    size_t{sizeof(uint64_t)},  //
};

inline constexpr MessageDefinition kMessagePong{
    "pong",                 //
    NetMessageType::kPong,  //
    false,
    size_t{0},
    size_t{0},
    size_t{sizeof(uint64_t)},  //
    size_t{sizeof(uint64_t)},  //
};

inline constexpr MessageDefinition kMessageGetheaders{
    "getheaders",                                                     //
    NetMessageType::kGetheaders,                                      //
    true,                                                             // vectorized
    size_t{2000},                                                     // max vector items
    size_t{32},                                                       // vector item size
    size_t{4 + 1 + 32 + 32},                                          // min payload length
    size_t{4 + serialization::ser_compact_sizeof(2000) + 32 * 2001},  // max payload length
};

inline constexpr MessageDefinition kMessageHeaders{
    "headers",                 //
    NetMessageType::kHeaders,  //
    true,                      // vectorized
    size_t{160},               // max vector items
    std::nullopt,              // vector item size
    size_t{1 + 140},           // min payload length
    std::nullopt,              // max payload length
};

inline constexpr MessageDefinition kMessageGetAddr{
    "getaddr",                 //
    NetMessageType::kGetaddr,  //
    false,                     // vectorized
    std::nullopt,              // max vector items
    std::nullopt,              // vector item size
    size_t{0},                 // min payload length
    size_t{0},                 // max payload length
};

inline constexpr MessageDefinition kMessageMempool{
    "mempool",                 //
    NetMessageType::kMemPool,  //
    false,
    std::nullopt,
    std::nullopt,
    size_t{0},
    size_t{0},
};

inline constexpr MessageDefinition kMessageMissingOrUnknown{
    nullptr,                            //
    NetMessageType::kMissingOrUnknown,  //
    false,
    size_t{0},
    size_t{0},
    size_t{0},  //
    size_t{0},  //
};

//! \brief List of all supported messages
//! \attention This must be kept in same order as the MessageCommand enum
inline constexpr std::array<MessageDefinition, 11> kMessageDefinitions{
    kMessageVersion,           // 0
    kMessageVerack,            // 1
    kMessageInv,               // 2
    kMessageAddr,              // 3
    kMessagePing,              // 4
    kMessagePong,              // 5
    kMessageGetheaders,        // 6
    kMessageHeaders,           // 7
    kMessageGetAddr,           // 8
    kMessageMempool,           // 9
    kMessageMissingOrUnknown,  // 10
};

static_assert(kMessageDefinitions.size() == static_cast<size_t>(NetMessageType::kMissingOrUnknown) + 1,
              "kMessageDefinitions must be kept in same order as the MessageCommand enum");

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    std::array<uint8_t, 4> network_magic{};     // Message magic (origin network)
    std::array<uint8_t, 12> command{};          // ASCII string identifying the packet content, NULL padded
    uint32_t payload_length{0};                 // Length of payload in bytes
    std::array<uint8_t, 4> payload_checksum{};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    //! \brief Returns the message definition
    [[nodiscard]] const MessageDefinition& get_definition() const noexcept;

    //! \brief Returns the decoded message type
    [[nodiscard]] NetMessageType get_type() const noexcept { return get_definition().message_type; }

    //! \brief Sets the message type and fills the command field
    //! \remarks On non pristine headers, this function has no effect
    void set_type(NetMessageType type) noexcept;

    //! \brief Reset the header to its factory state
    void reset() noexcept;

    //! \brief Check if the header is in its factory state
    [[nodiscard]] bool pristine() const noexcept;

    //! \brief Performs a sanity check on the header
    [[nodiscard]] serialization::Error validate() noexcept;

  private:
    NetMessageType message_type_{NetMessageType::kMissingOrUnknown};
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NetMessage {
  public:
    //! \brief Construct a blank NetMessage
    NetMessage() : ser_stream_{serialization::Scope::kNetwork, 0} {};

    //! \brief Construct a blank NetMessage with a specific version
    explicit NetMessage(int version) : ser_stream_{serialization::Scope::kNetwork, version} {};

    //! \brief Construct a NetMessage with network magic provided
    explicit NetMessage(int version, std::array<uint8_t, 4>& magic)
        : ser_stream_{serialization::Scope::kNetwork, version} {
        header_.network_magic = magic;
    };

    // Not movable nor copyable
    NetMessage(const NetMessage&) = delete;
    NetMessage(const NetMessage&&) = delete;
    NetMessage& operator=(const NetMessage&) = delete;
    ~NetMessage() = default;

    [[nodiscard]] size_t size() const noexcept { return ser_stream_.size(); }
    [[nodiscard]] NetMessageType get_type() const noexcept { return header_.get_type(); }
    [[nodiscard]] NetMessageHeader& header() noexcept { return header_; }
    [[nodiscard]] serialization::SDataStream& data() noexcept { return ser_stream_; }
    [[nodiscard]] serialization::Error parse(ByteView& input_data, ByteView network_magic = {}) noexcept;

    //! \brief Sets the message version (generally inherited from the protocol version)
    void set_version(int version) noexcept;

    //! \brief Returns the message version
    [[nodiscard]] int get_version() const noexcept;

    //! \brief Validates the message header, payload and checksum
    [[nodiscard]] serialization::Error validate() noexcept;

    //! \brief Populates the message header and payload
    serialization::Error push(NetMessageType message_type, serialization::Serializable& payload,
                              ByteView magic) noexcept;

    //! \brief Populates the message header and payload (alias)
    serialization::Error push(NetMessageType message_type, serialization::Serializable& payload,
                              std::array<uint8_t, 4>& magic) noexcept;

  private:
    NetMessageHeader header_{};              // Where the message header is deserialized
    serialization::SDataStream ser_stream_;  // Contains all the message raw data

    [[nodiscard]] serialization::Error validate_checksum() noexcept;
};

}  // namespace zenpp::abi
