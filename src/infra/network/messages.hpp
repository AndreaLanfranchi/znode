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

namespace zenpp::net {

static constexpr size_t kMaxProtocolMessageLength{static_cast<size_t>(4_MiB)};  // Maximum length of a protocol message
static constexpr size_t kMessageHeaderLength{24};                               // Length of a protocol message header
static constexpr size_t kMaxInvItems{50'000};                                   // Maximum number of inventory items
static constexpr size_t kInvItemSize{36};            // Size of an inventory item (type + hash)
static constexpr size_t kMaxAddrItems{1'000};        // Maximum number of items in an addr message
static constexpr size_t kAddrItemSize{30};           // Size of an address item (time + services + ip + port)
static constexpr size_t kMaxGetHeadersItems{2'000};  // Maximum number of block headers in a getheaders message
static constexpr size_t kMaxHeadersItems{160};       // Maximum number of block headers in a headers message

enum class MessageType : uint32_t {
    kVersion,           // Dial-out nodes async_send their version first
    kVerAck,            // Reply by dial-in nodes to version message
    kInv,               // Inventory message to advertise the knowledge of hashes of blocks or transactions
    kAddr,              // Address message to advertise the knowledge of addresses of other nodes
    kPing,              // Ping message to measure the latency of a connection
    kPong,              // Pong message to reply to a ping message
    kGetHeaders,        // Getheaders message to request/async_send a list of block headers
    kHeaders,           // Headers message to async_send a list of block
    kGetAddr,           // Getaddr message to request a list of known active peers
    kMemPool,           // MemPool message to request/async_send a list of transactions in the mempool
    kMissingOrUnknown,  // This must be the last entry
};

struct MessageDefinition {
    const char* command{nullptr};                              // The command string
    MessageType message_type{MessageType::kMissingOrUnknown};  // The command id
    const bool is_vectorized{false};                           // Whether the payload is a vector of items
    const std::optional<size_t> max_vector_items{};            // The maximum number of vector items in the payload
    const std::optional<size_t> vector_item_size{};            // The size of a vector item
    const std::optional<size_t> min_payload_length{};          // The min allowed payload length
    const std::optional<size_t> max_payload_length{};          // The max allowed payload length
    const std::optional<int> min_protocol_version{};           // The min protocol version that supports this message
    const std::optional<int> max_protocol_version{};           // The max protocol version that supports this message
};

inline constexpr MessageDefinition kMessageVersion{
    .command = "version",
    .message_type = MessageType::kVersion,
    .min_payload_length = size_t{46},
    .max_payload_length = size_t(1_KiB),
};

inline constexpr MessageDefinition kMessageVerack{
    .command = "verack",
    .message_type = MessageType::kVerAck,
    .min_payload_length = size_t{0},
    .max_payload_length = size_t{0},
};

inline constexpr MessageDefinition kMessageInv{
    .command = "inv",
    .message_type = MessageType::kInv,
    .is_vectorized = true,
    .max_vector_items = size_t{kMaxInvItems},
    .vector_item_size = size_t{kInvItemSize},
    .min_payload_length = size_t{1 + kInvItemSize},
    .max_payload_length = size_t{ser::ser_compact_sizeof(kMaxInvItems) + (kMaxInvItems * kInvItemSize)},
};

inline constexpr MessageDefinition kMessageAddr{
    .command = "addr",
    .message_type = MessageType::kAddr,
    .is_vectorized = true,
    .max_vector_items = size_t{kMaxAddrItems},
    .vector_item_size = size_t{kAddrItemSize},
    .min_payload_length = size_t{1 + kAddrItemSize},
    .max_payload_length = size_t{ser::ser_compact_sizeof(kMaxAddrItems) + (kMaxAddrItems * kAddrItemSize)},
};

inline constexpr MessageDefinition kMessagePing{
    .command = "ping",
    .message_type = MessageType::kPing,
    .min_payload_length = size_t{sizeof(uint64_t)},
    .max_payload_length = size_t{sizeof(uint64_t)},
};

inline constexpr MessageDefinition kMessagePong{
    .command = "pong",
    .message_type = MessageType::kPong,
    .min_payload_length = size_t{sizeof(uint64_t)},
    .max_payload_length = size_t{sizeof(uint64_t)},
};

inline constexpr MessageDefinition kMessageGetheaders{
    .command = "getheaders",
    .message_type = MessageType::kGetHeaders,
    .is_vectorized = true,
    .max_vector_items = size_t{kMaxGetHeadersItems},
    .vector_item_size = size_t{h256::size()},
    .min_payload_length = size_t{/*version*/ 4 + /*count*/ 1 + h256::size() * /* known + stop */ 2},
    .max_payload_length = size_t{/*version*/ 4 + /*version*/ ser::ser_compact_sizeof(kMaxGetHeadersItems) +
                                 h256::size() * (/* known + stop */ kMaxGetHeadersItems + 1)},  // max payload length
};

inline constexpr MessageDefinition kMessageHeaders{
    .command = "headers",
    .message_type = MessageType::kHeaders,
    .is_vectorized = true,
    .max_vector_items = size_t{kMaxHeadersItems},
    .min_payload_length = size_t{1 + 140},
};

inline constexpr MessageDefinition kMessageGetAddr{
    .command = "getaddr",
    .message_type = MessageType::kGetAddr,
    .min_payload_length = size_t{0},
    .max_payload_length = size_t{0},
};

inline constexpr MessageDefinition kMessageMempool{
    .command = "mempool",
    .message_type = MessageType::kMemPool,
    .min_payload_length = size_t{0},
    .max_payload_length = size_t{0},
};

inline constexpr MessageDefinition kMessageMissingOrUnknown{};

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

static_assert(kMessageDefinitions.size() == static_cast<size_t>(MessageType::kMissingOrUnknown) + 1,
              "kMessageDefinitions must be kept in same order as the MessageCommand enum");

}  // namespace zenpp::net
