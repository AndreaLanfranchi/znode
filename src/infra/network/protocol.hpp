/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <limits>

namespace zenpp::net {

static constexpr int32_t kDefaultProtocolVersion{170'002};                       // Our default protocol version
static constexpr int32_t kMinSupportedProtocolVersion{kDefaultProtocolVersion};  // Min acceptable protocol version
static constexpr int32_t kMaxSupportedProtocolVersion{kDefaultProtocolVersion};  // Max acceptable protocol version
static constexpr size_t kMaxProtocolMessageLength{static_cast<size_t>(4_MiB)};   // Maximum length of a protocol message
static constexpr size_t kMessageHeaderLength{24};                                // Length of a protocol message header
static constexpr size_t kMaxInvItems{50'000};                                    // Maximum number of inventory items
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

}  // namespace zenpp::net
