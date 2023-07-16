/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <algorithm>

#include <gsl/gsl_util>

#include <core/abi/netmessage.hpp>
#include <core/common/misc.hpp>
#include <core/crypto/hash256.hpp>

namespace zen {

void NetMessageHeader::reset() noexcept {
    magic.fill(0);
    command.fill(0);
    length = 0;
    checksum.fill(0);
    message_definition_id_ = static_cast<size_t>(NetMessageType::kMissingOrUnknown);
}

bool NetMessageHeader::pristine() const noexcept {
    return std::all_of(magic.begin(), magic.end(), [](const auto& byte) { return byte == 0; }) &&
           std::all_of(command.begin(), command.end(), [](const auto& byte) { return byte == 0; }) &&
           std::all_of(checksum.begin(), checksum.end(), [](const auto& byte) { return byte == 0; }) &&
           message_definition_id_ == static_cast<size_t>(NetMessageType::kMissingOrUnknown) && length == 0;
}

void NetMessageHeader::set_type(const NetMessageType type) noexcept {
    if (!pristine()) return;
    const auto& message_definition{kMessageDefinitions[static_cast<size_t>(type)]};
    const auto command_bytes{strnlen_s(message_definition.command, command.size())};
    memcpy(command.data(), message_definition.command, command_bytes);
    message_definition_id_ = static_cast<size_t>(type);
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

serialization::Error NetMessageHeader::validate(std::optional<ByteView> expected_network_magic) const noexcept {
    using enum serialization::Error;
    if (expected_network_magic && ByteView{magic} != *expected_network_magic) return kMessageHeaderMagicMismatch;
    if (command[0] == 0) return kMessageHeaderEmptyCommand;  // reject empty commands
    if (length > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

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
    size_t id{0};
    for (const auto& msg_def : kMessageDefinitions) {
        const auto cmp_len{strnlen_s(msg_def.command, command.size())};
        if (memcmp(msg_def.command, command.data(), cmp_len) == 0) {
            message_definition_id_ = id;
            break;
        }
        ++id;
    }

    if (message_definition_id_ == static_cast<size_t>(NetMessageType::kMissingOrUnknown))
        return kMessageHeaderUnknownCommand;

    const auto& message_definition{kMessageDefinitions[message_definition_id_]};
    if (message_definition.min_payload_length.has_value() && length < *message_definition.min_payload_length) {
        return kMessageHeaderUndersizedPayload;
    }
    if (message_definition.max_payload_length.has_value() && length > *message_definition.max_payload_length) {
        return kMessageHeaderOversizedPayload;
    }

    if (length == 0) /* Hash of empty payload is already known */
    {
        auto empty_payload_hash{crypto::Hash256::kEmptyHash()};
        if (memcmp(checksum.data(), empty_payload_hash.data(), checksum.size()) != 0)
            return kMessageHeaderInvalidChecksum;
    }

    return kSuccess;
}

serialization::Error NetMessage::validate() const noexcept {
    using enum serialization::Error;

    // Being a network message the payload size MUST NOT exceed the maximum allowed size for the protocol
    // regardless any other check
    if (raw_data_->size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (raw_data_->size() > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    // This also means the header has not been validated previously
    const auto message_type(header_->get_type());
    if (message_type == NetMessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;

    if (raw_data_->size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (raw_data_->size() < kMessageHeaderLength + header_->length) return kMessageBodyIncomplete;
    if (raw_data_->size() > kMessageHeaderLength + header_->length) return kMessageMismatchingPayloadLength;

    // From here on ensure we return to the beginning of the payload
    const auto return_to_beginning{gsl::finally([this] { raw_data_->seekg(kMessageHeaderLength); })};

    // Validate payload : length and checksum
    raw_data_->seekg(kMessageHeaderLength);  // Important : skip the header !!!
    if (raw_data_->avail() != header_->length) return kMessageMismatchingPayloadLength;
    auto payload_view{raw_data_->read()};
    if (!payload_view) return payload_view.error();
    if (auto error{validate_payload_checksum(*payload_view, header_->checksum)}; error != kSuccess) return error;

    // For specific messages the vectorized data size can be known in advance
    // e.g. inventory messages are made of 36 bytes elements hence, after the initial
    // read of the vector size the payload size can be checked against the expected size
    const auto& message_definition{kMessageDefinitions[static_cast<size_t>(message_type)]};
    if (message_definition.max_vector_items.has_value()) {
        raw_data_->seekg(kMessageHeaderLength);
        const auto vector_size{serialization::read_compact(*raw_data_)};
        if (!vector_size) return vector_size.error();
        if (*vector_size == 0) return kMessagePayloadEmptyVector;  // MUST have some item
        if (*vector_size > *message_definition.max_vector_items) return kMessagePayloadOversizedVector;
        if (message_definition.vector_item_size.has_value()) {
            const auto expected_vector_size{*vector_size * *message_definition.vector_item_size};
            if (raw_data_->avail() != expected_vector_size) return kMessagePayloadMismatchesVectorSize;
            // Look for duplicates
            payload_view = raw_data_->read();
            ZEN_ASSERT(payload_view);
            if (const auto duplicate_count{count_duplicate_data_chunks(
                    *payload_view, *message_definition.vector_item_size, 1 /* one is enough */)};
                duplicate_count > 0) {
                return kMessagePayloadDuplicateVectorItems;
            }
        }
    }

    return kSuccess;
}

serialization::Error NetMessage::validate_payload_checksum(ByteView payload, ByteView expected_checksum) noexcept {
    using enum serialization::Error;
    static crypto::Hash256 payload_digest{};
    payload_digest.init(payload);
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), expected_checksum.data(), expected_checksum.size()) != 0) {
        return kMessageHeaderInvalidChecksum;
    }
    return kSuccess;
}
}  // namespace zen
