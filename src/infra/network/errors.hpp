/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once
#include <cstdint>
#include <string>

#include <boost/system/error_code.hpp>
#include <magic_enum.hpp>

namespace znode::net {

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
    kMessagePayloadExtraData,                   // Message payload contains unparseable extra data
    kMessagePayLoadUnhandleable,                // Message payload is unhandleable (we're missing a handler)
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
    kUnknownRejectedCommand,                    // A rejection message rejects an unknown command
    kMessagePayloadInvalidLastBlockHeight,      // Message payload's last block height is invalid (Version Message)
    kMessagePayloadInvalidTimestamp,            // Message payload's timestamp is invalid (Version Message)
    kInvalidNtpResponse,                        // Invalid NTP response
    kInvalidSystemTime,                         // Invalid system time
};

#ifdef __GNUC__
// boost::system::error_category has overridable members but no virtual dtor
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

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
            case kMessagePayloadInvalidLastBlockHeight:
            case kMessagePayloadInvalidTimestamp:
            case kInvalidSystemTime:
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
            case kInvalidNtpResponse:
                return make_error_condition(boost::system::errc::protocol_error);
            default:
                return {err_code, *this};
        }
    }
};

#ifdef __GNUC__
// boost::system::error_category has overridable members but no virtual dtor
#pragma GCC diagnostic pop
#endif

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline boost::system::error_code make_error_code(net::Error err_code) {
    static net::ErrorCategory category{};
    return {static_cast<int>(err_code), category};
}
}  // namespace znode::net

namespace boost::system {
// Tell the C++ 11 STL metaprogramming that our enums are registered with the
// error code system
template <>
struct is_error_code_enum<znode::net::Error> : public std::true_type {};
}  // namespace boost::system
