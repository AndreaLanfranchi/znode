/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <core/common/base.hpp>
#include <core/serialization/serialize.hpp>
#include <core/types/hash.hpp>

namespace zenpp::abi {

static constexpr size_t kMaxProtocolMessageLength{static_cast<size_t>(4_MiB)};  // Maximum length of a protocol message
static constexpr size_t kMessageHeaderLength{24};                               // Length of a protocol message header
static constexpr size_t kMaxInvItems{50'000};                                   // Maximum number of inventory items
static constexpr size_t kInvItemSize{36};            // Size of an inventory item (type + hash)
static constexpr size_t kMaxAddrItems{1'000};        // Maximum number of items in an addr message
static constexpr size_t kAddrItemSize{30};           // Size of an address item (time + services + ip + port)
static constexpr size_t kMaxGetHeadersItems{2'000};  // Maximum number of block headers in a getheaders message
static constexpr size_t kMaxHeadersItems{160};       // Maximum number of block headers in a headers message

enum class NetMessageType : uint32_t {
    kVersion,           // Dial-out nodes send their version first
    kVerAck,            // Reply by dial-in nodes to version message
    kInv,               // Inventory message to advertise the knowledge of hashes of blocks or transactions
    kAddr,              // Address message to advertise the knowledge of addresses of other nodes
    kPing,              // Ping message to measure the latency of a connection
    kPong,              // Pong message to reply to a ping message
    kGetHeaders,        // Getheaders message to request/send a list of block headers
    kHeaders,           // Headers message to send a list of block
    kGetAddr,           // Getaddr message to request a list of known active peers
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
    NetMessageType::kVerAck,  //
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
    "getheaders",                                                               //
    NetMessageType::kGetHeaders,                                                //
    true,                                                                       // vectorized
    size_t{kMaxGetHeadersItems},                                                // max vector items
    size_t{h256::size()},                                                       // vector item size
    size_t{/*version*/ 4 + /*count*/ 1 + h256::size() * /* known + stop */ 2},  // min payload length
    size_t{/*version*/ 4 + /*version*/ serialization::ser_compact_sizeof(kMaxGetHeadersItems) +
           h256::size() * (/* known + stop */ kMaxGetHeadersItems + 1)},  // max payload length
};

inline constexpr MessageDefinition kMessageHeaders{
    "headers",                 //
    NetMessageType::kHeaders,  //
    true,                      // vectorized
    size_t{kMaxHeadersItems},  // max vector items
    std::nullopt,              // vector item size
    size_t{1 + 140},           // min payload length
    std::nullopt,              // max payload length
};

inline constexpr MessageDefinition kMessageGetAddr{
    "getaddr",                 //
    NetMessageType::kGetAddr,  //
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

}  // namespace zenpp::abi
