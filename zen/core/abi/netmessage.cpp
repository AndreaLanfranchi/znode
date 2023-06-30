/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "netmessage.hpp"

namespace zen {

void NetMessageHeader::reset() noexcept {
    magic = 0;
    command.fill(0);
    length = 0;
    checksum.fill(0);
}

serialization::Error NetMessageHeader::serialization(serialization::SDataStream& stream, serialization::Action action) {
    using namespace serialization;
    using enum Error;
    Error error{stream.bind(magic, action)};
    if (!error) error = stream.bind(command, action);
    if (!error) error = stream.bind(length, action);
    if (!error) error = stream.bind(checksum, action);
    return error;
}

serialization::Error NetMessageHeader::validate(std::optional<uint32_t> expected_magic) const noexcept {
    using enum serialization::Error;
    if (expected_magic && magic != *expected_magic) return kMessageHeaderMagicMismatch;
    if (command[0] == 0) return kMessageHeaderEmptyCommand;  // reject empty commands

    // Check the command string is made of printable characters
    // eventually right padded to 12 bytes with NUL (0x00) characters.
    bool null_matched{false};
    for (const auto c : command) {
        if (!null_matched) {
            if (c == 0) {
                null_matched = true;
                continue;
            }
            if (c < 32 || c > 126) return kMessageHeaderMalformedCommand;
        } else if (c) {
            return kMessageHeaderMalformedCommand;
        }
    }

    // Identify the command amongst the known ones
    for (const auto& msg_def : kMessageDefinitions) {
        const auto cmp_len{strnlen_s(msg_def.command, 12)};
        if (memcmp(msg_def.command, command.data(), cmp_len) == 0) {
            message_type = msg_def.message_type;  // Found the command

            // Check max size of payload if applicable
            if (msg_def.max_payload_length.has_value() && length > *msg_def.max_payload_length) {
                return kMessageHeaderOversizedPayload;
            }
            max_payload_length_ = msg_def.max_payload_length;
            break;
        }
    }

    if (message_type == MessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;
    if (length > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    return kSuccess;
}
}  // namespace zen
