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

#include <infra/network/protocol.hpp>

namespace zenpp::net {

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

inline constexpr MessageDefinition kMessageVerAck{
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

inline constexpr MessageDefinition kMessageReject{
    .command = "reject",
    .message_type = MessageType::kReject,
    .min_payload_length = size_t{3},
    .max_payload_length = size_t{/*rejected command*/ 12 + /*code*/ 1 + /*reason*/ 256 + /*extra_data*/ 32},
};

inline constexpr MessageDefinition kMessageMissingOrUnknown{};

//! \brief List of all supported messages
//! \attention This must be kept in same order as the MessageCommand enum
inline constexpr std::array<MessageDefinition, 12> kMessageDefinitions{
    kMessageVersion,           // 0
    kMessageVerAck,            // 1
    kMessageInv,               // 2
    kMessageAddr,              // 3
    kMessagePing,              // 4
    kMessagePong,              // 5
    kMessageGetheaders,        // 6
    kMessageHeaders,           // 7
    kMessageGetAddr,           // 8
    kMessageMempool,           // 9
    kMessageReject,            // 10
    kMessageMissingOrUnknown,  // 11
};

static_assert(kMessageDefinitions.size() == static_cast<size_t>(MessageType::kMissingOrUnknown) + 1,
              "kMessageDefinitions must be kept in same order as the MessageCommand enum");

}  // namespace zenpp::net
