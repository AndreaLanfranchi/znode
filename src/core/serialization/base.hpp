/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>
#include <cstdint>
#include <ranges>

namespace zenpp::serialization {

static constexpr uint32_t kMaxSerializedCompactSize{0x02000000};

//! \brief Scopes of serialization/deserialization
enum class Scope : uint32_t {
    kNetwork = (1 << 0),
    kStorage = (1 << 1),
    kHash = (1 << 2)
};

//! \brief Specifies the serialization action
enum class Action : uint32_t {
    kComputeSize = (1 << 0),  // Only calculates size of serialized data
    kSerialize = (1 << 1),    // Actually performs serialization
    kDeserialize = (1 << 2)   // Deserializes data into object
};

enum class Error {
    kSuccess,  // Actually not an error
    kOverflow,
    kReadBeyondData,
    kNonCanonicalCompactSize,
    kCompactSizeTooBig,
    kInvalidMessageState,                       // Trying to push a payload in an already initialized network message
    kMessageHeaderIncomplete,                   // Message header too short and cannot be completed
    kMessageBodyIncomplete,                     // Message body too short
    kMessageHeaderMagicMismatch,                // Message header addressed to another network
    kMessageHeaderEmptyCommand,                 // Message header command is empty
    kMessageHeaderUnknownCommand,               // Message header command is unknown
    kMessageHeaderMalformedCommand,             // Message header's command is malformed (e.g. not null padded)
    kMessageHeaderUndersizedPayload,            // Message header's declared payload size is too small
    kMessageHeaderOversizedPayload,             // Message header's declared payload size is too wide
    kMessageMismatchingPayloadLength,           // Message payload length does not match the declared one
    kMessageHeaderInvalidChecksum,              // Message header's checksum is invalid
    kMessagePayloadEmptyVector,                 // Message payload vector is empty
    kMessagePayloadOversizedVector,             // Message payload vector is too large
    kMessagePayloadMismatchesVectorSize,        // Message payload vector size does not match the declared one
    kMessagePayloadDuplicateVectorItems,        // Message payload vector contains duplicate items
    KMessagesFloodingDetected,                  // Message flooding detected
    kInvalidProtocolHandShake,                  // Wrong message sequence detected
    kInvalidProtocolVersion,                    // Wrong protocol version detected
    kDuplicateProtocolHandShake,                // Duplicate handshake message detected
    kUnsupportedMessageTypeForProtocolVersion,  // Message type is not supported in current protocol version
    kDeprecatedMessageTypeForProtocolVersion,   // Message type is deprecated in current protocol version
    kUndefinedError,                            // Not defined
};

inline bool operator!(Error e) { return e == static_cast<Error>(0); }

inline constexpr std::array<Error, 3> kNonFatalErrors{Error::kSuccess, Error::kMessageHeaderIncomplete,
                                                      Error::kMessageBodyIncomplete};

inline bool is_fatal_error(Error e) { return std::ranges::find(kNonFatalErrors, e) == kNonFatalErrors.end(); }

}  // namespace zenpp::serialization
