/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <string>

#include <boost/system/error_code.hpp>
#include <magic_enum.hpp>

namespace zenpp::net {

enum class Error {
    kSuccess,                                   // Not actually an error
    kMessageSizeOverflow,                       // Message size overflow
    kMessageHeaderIncomplete,                   // Message header is incomplete (need more data)
    kMessageBodyIncomplete,                     // Message body is incomplete (need more data)
    kMessageHeaderInvalidMagic,                 // Message header's magic field is invalid
    kMessageHeaderMalformedCommand,             // Message header's command field is malformed
    kMessageHeaderEmptyCommand,                 // Message header's command field is empty
    kMessageHeaderIllegalCommand,               // Message header's command field is not a valid command
    kMessageHeaderIllegalPayloadLength,         // Message header's payload length is not allowed
    kMessageHeaderInvalidChecksum,              // Message header's checksum is invalid
    kMessagePayloadEmptyVector,                 // Message payload's expected vectorized, but no items provided
    kMessagePayloadOversizedVector,             // Message payload's expected vectorized, but too many items provided
    kMessagePayloadLengthMismatchesVectorSize,  // Message payload's vectorized, but size mismatches
    kMessagePayloadDuplicateVectorItems,        // Message payload's vectorized, but contains duplicate items
    kMessageUnknownCommand,                     // Message command is unknown
    kMessageWriteNotPermitted,                  // Message write is not permitted (message is already complete)
    kMessagePushNotPermitted,                   // Message push is not permitted (already initialized header)
    kInvalidProtocolVersion,                    // Wrong protocol version detected
    kUnsupportedMessageTypeForProtocolVersion,  // Message type is not supported by the protocol version
    kDeprecatedMessageTypeForProtocolVersion,   // Message type is deprecated by the protocol version
    kDuplicateProtocolHandShake,                // Duplicate handshake message detected
    kInvalidProtocolHandShake,                  // Wrong message sequence detected
    kMessageFloodingDetected,                   // Message flooding detected
    kConnectedToSelf,                           // Connected to self
    kUnsolicitedPong,                           // Unsolicited pong message
    kInvalidPingPongNonce,                      // Ping nonce mismatch
};

class ErrorCategory : public boost::system::error_category {
  public:
    virtual ~ErrorCategory() noexcept = default;
    const char* name() const noexcept override { return "NetworkError"; }
    std::string message(int err_code) const override {
        std::string desc{"Unknown error"};
        if (const auto enumerator = magic_enum::enum_cast<net::Error>(err_code); enumerator.has_value()) {
            desc.assign(std::string(magic_enum::enum_name<net::Error>(enumerator.value())));
            desc.erase(0, 1);  // Remove the constant `k` prefix
        }
        return desc;
    }
    boost::system::error_condition default_error_condition(int err_code) const noexcept override {
        const auto enumerator = magic_enum::enum_cast<net::Error>(err_code);
        if (not enumerator.has_value()) {
            return {err_code, *this};  // No conversion
        }
        switch (*enumerator) {
            using enum Error;
            case kSuccess:
                return make_error_condition(boost::system::errc::success);
            case kMessageHeaderIncomplete:
            case kMessageBodyIncomplete:
                return make_error_condition(boost::system::errc::operation_in_progress);
            case kMessageSizeOverflow:
            case kMessageHeaderIllegalPayloadLength:
            case kMessagePayloadEmptyVector:
            case kMessagePayloadOversizedVector:
            case kMessagePayloadLengthMismatchesVectorSize:
                return make_error_condition(boost::system::errc::message_size);
            case kInvalidProtocolVersion:
            case kMessageHeaderMalformedCommand:
            case kMessageHeaderEmptyCommand:
            case kMessageUnknownCommand:
            case kMessagePayloadDuplicateVectorItems:
            case kInvalidPingPongNonce:
                return make_error_condition(boost::system::errc::invalid_argument);
            case kMessageHeaderIllegalCommand:
            case kMessageHeaderInvalidChecksum:
            case kMessageHeaderInvalidMagic:
                return make_error_condition(boost::system::errc::argument_out_of_domain);
            case kMessagePushNotPermitted:
            case kMessageFloodingDetected:
            case kConnectedToSelf:
            case kUnsolicitedPong:
                return make_error_condition(boost::system::errc::operation_not_permitted);
            case kUnsupportedMessageTypeForProtocolVersion:
            case kDeprecatedMessageTypeForProtocolVersion:
            case kDuplicateProtocolHandShake:
            case kInvalidProtocolHandShake:
                return make_error_condition(boost::system::errc::protocol_error);
            default:
                return {err_code, *this};
        }
    }
};

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline boost::system::error_code make_error_code(net::Error err_code) {
    static net::ErrorCategory category{};
    return {static_cast<int>(err_code), category};
}
}  // namespace zenpp::net

namespace boost::system {
// Tell the C++ 11 STL metaprogramming that our enums are registered with the
// error code system
template <>
struct is_error_code_enum<zenpp::net::Error> : public std::true_type {};
}  // namespace boost::system
