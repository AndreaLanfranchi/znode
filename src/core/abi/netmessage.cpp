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

namespace zenpp {

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
    Error err{Error::kSuccess};
    if (action == Action::kSerialize) err = validate();
    if (!err) err = stream.bind(magic, action);
    if (!err) err = stream.bind(command, action);
    if (!err) err = stream.bind(length, action);
    if (!err) err = stream.bind(checksum, action);
    if (!err && action == Action::kDeserialize) err = validate();
    return err;
}

serialization::Error NetMessageHeader::validate() const noexcept {
    using namespace serialization;
    using enum Error;
    if (command[0] == 0) return kMessageHeaderEmptyCommand;  // reject empty commands
    if (length > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    // Check the command string is made of printable characters
    // eventually right padded to 12 bytes with NUL (0x00) characters.
    bool null_matched{false};
    size_t got_command_len{0};
    for (const auto c : command) {
        if (!null_matched) {
            if (c == 0) {
                null_matched = true;
                continue;
            }
            if (c < 32 || c > 126) return kMessageHeaderMalformedCommand;
            ++got_command_len;
        } else if (c) {
            return kMessageHeaderMalformedCommand;
        }
    }

    // Identify the command amongst the known ones
    decltype(message_definition_id_) id{0};
    for (const auto& msg_def : kMessageDefinitions) {
        const auto def_command_len{strnlen_s(msg_def.command, command.size())};
        if (got_command_len == def_command_len && memcmp(msg_def.command, command.data(), def_command_len) == 0) {
            message_definition_id_ = id;
            break;
        }
        ++id;
    }

    if (message_definition_id_ == static_cast<size_t>(NetMessageType::kMissingOrUnknown))
        return kMessageHeaderUnknownCommand;

    const auto& message_definition{get_definition()};
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

    // Being a network message the payload :
    // - must be at least kMessageHeaderLength byte long
    // - must be at most kMaxProtocolMessageLength byte long
    if (data_.size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (data_.size() > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    // This also means the header has not been validated previously
    const auto& message_definition(header_.get_definition());
    if (message_definition.message_type == NetMessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;

    if (data_.size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (data_.size() < kMessageHeaderLength + header_.length) return kMessageBodyIncomplete;
    if (data_.size() > kMessageHeaderLength + header_.length) return kMessageMismatchingPayloadLength;

    // From here on ensure we return to the beginning of the payload
    const auto data_to_payload{gsl::finally([this] { data_.seekg(kMessageHeaderLength); })};

    // Validate payload : length and checksum
    data_.seekg(kMessageHeaderLength);  // Important : skip the header !!!
    if (data_.avail() != header_.length) return kMessageMismatchingPayloadLength;
    auto payload_view{data_.read()};
    if (!payload_view) return payload_view.error();
    if (auto error{validate_checksum()}; error != kSuccess) return error;

    // For specific messages the vectorized data size can be known in advance
    // e.g. inventory messages are made of 36 bytes elements hence, after the initial
    // read of the vector size the payload size can be checked against the expected size
    if (message_definition.is_vectorized) {
        data_.seekg(kMessageHeaderLength);
        const auto vector_size{serialization::read_compact(data_)};
        if (!vector_size) return vector_size.error();
        if (*vector_size == 0) return kMessagePayloadEmptyVector;  // MUST have at least 1 element
        if (*vector_size > message_definition.max_vector_items.value_or(UINT32_MAX))
            return kMessagePayloadOversizedVector;
        if (message_definition.vector_item_size.has_value()) {
            const auto expected_vector_size{*vector_size * *message_definition.vector_item_size};
            if (data_.avail() != expected_vector_size) return kMessagePayloadMismatchesVectorSize;
            // Look for duplicates
            payload_view = data_.read();
            ASSERT(payload_view);
            if (const auto duplicate_count{count_duplicate_data_chunks(
                    *payload_view, *message_definition.vector_item_size, 1 /* one is enough */)};
                duplicate_count > 0) {
                return kMessagePayloadDuplicateVectorItems;
            }
        }
    }

    return kSuccess;
}

serialization::Error NetMessage::parse(ByteView& input_data) noexcept {
    using namespace serialization;
    using enum Error;

    Error ret{kSuccess};
    while (!input_data.empty()) {
        const bool header_mode(data_.tellg() < kMessageHeaderLength);
        auto bytes_to_read(header_mode ? kMessageHeaderLength - data_.avail() : header_.length - data_.avail());
        if (bytes_to_read > input_data.size()) bytes_to_read = input_data.size();
        data_.write(input_data.substr(0, bytes_to_read));
        input_data.remove_prefix(bytes_to_read);

        if (header_mode) {
            if (data_.avail() < kMessageHeaderLength) {
                ret = kMessageHeaderIncomplete;  // Not enough data for a full header
                break;                           // All data consumed nevertheless
            }

            ret = header_.deserialize(data_);
            if (ret == kSuccess) {
                const auto& message_definition{header_.get_definition()};
                if (message_definition.min_protocol_version.has_value() &&
                    data_.get_version() < *message_definition.min_protocol_version) {
                    ret = kUnsupportedMessageTypeForProtocolVersion;
                }
                if (message_definition.max_protocol_version.has_value() &&
                    data_.get_version() > *message_definition.max_protocol_version) {
                    ret = kDeprecatedMessageTypeForProtocolVersion;
                }
            }
            if (ret == kSuccess) ret = header_.validate(/* TODO Network magic here */);
            if (ret == kSuccess) {
                if (header_.length == 0) return validate_checksum();  // No payload to read
                continue;                                             // Keep reading the body payload - if any
            }
            break;  // Exit on any error - here are all fatal

        } else {
            if (data_.avail() < header_.length) {
                ret = kMessageBodyIncomplete;  // Not enough data for a full body
                break;                         // All data consumed nevertheless
            }
            ret = validate();  // Validate the whole payload of the message
            break;             // Exit anyway as either there is an or we have consumed all input data
        }
    }

    return ret;
}

serialization::Error NetMessage::validate_checksum() const noexcept {
    using enum serialization::Error;
    const auto current_pos{data_.tellg()};
    if (data_.seekg(kMessageHeaderLength) != kMessageHeaderLength) return kMessageHeaderIncomplete;
    const auto data_to_payload{gsl::finally([this, current_pos] { std::ignore = data_.seekg(current_pos); })};

    const auto payload_view{data_.read()};
    if (!payload_view) return payload_view.error();

    serialization::Error ret{kSuccess};
    crypto::Hash256 payload_digest(*payload_view);
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), header_.checksum.data(), header_.checksum.size()) != 0) {
        ret = kMessageHeaderInvalidChecksum;
    }
    return ret;
}
}  // namespace zenpp
