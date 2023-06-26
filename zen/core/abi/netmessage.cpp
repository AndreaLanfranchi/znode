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
    checksum = 0;
}

serialization::Error NetMessageHeader::serialization(serialization::DataStream& archive, serialization::Action action) {
    using namespace serialization;
    using enum Error;
    Error error{archive.bind(magic, action)};
    if (!error) error = archive.bind(command, action);
    if (!error) error = archive.bind(length, action);
    if (!error) error = archive.bind(checksum, action);
    return error;
}
bool NetMessageHeader::is_valid(std::optional<uint32_t> expected_magic) const noexcept {
    if (expected_magic && magic != *expected_magic) return false;
    if (command[0] == 0) return false;  // reject empty commands

    // Check the command string is made of printable characters
    // eventually right padded to 12 bytes with NUL (0x00) characters.
    bool null_matched{false};
    for (const auto c : command) {
        if (!null_matched) {
            if (c == 0) {
                null_matched = true;
                continue;
            }
            if (c < 32 || c > 126) return false;
        } else if (c) {
            return false;
        }
    }

    if (length > kMaxProtocolMessageLength) return false;

    return true;
}
}  // namespace zen
