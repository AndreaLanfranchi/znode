/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "message.hpp"

#include "core/common/misc.hpp"

#include <algorithm>

#include <gsl/gsl_util>

namespace zenpp::net {

void MessageHeader::reset() noexcept {
    network_magic.fill('0');
    command.fill('0');
    payload_length = 0;
    payload_checksum.fill('0');
    message_type_ = MessageType::kMissingOrUnknown;
}

bool MessageHeader::pristine() const noexcept {
    return std::ranges::all_of(network_magic, [](const auto ubyte) { return ubyte == 0U; }) &&
           std::ranges::all_of(command, [](const auto ubyte) { return ubyte == 0U; }) &&
           std::ranges::all_of(payload_checksum, [](const auto ubyte) { return ubyte == 0U; }) &&
           message_type_ == MessageType::kMissingOrUnknown && payload_length == 0;
}

void MessageHeader::set_type(const MessageType type) noexcept {
    if (!pristine()) return;
    const auto& message_definition{kMessageDefinitions[static_cast<size_t>(type)]};
    const auto command_bytes{strnlen_s(message_definition.command, command.size())};
    memcpy(command.data(), message_definition.command, command_bytes);
    message_type_ = type;
}

outcome::result<void> MessageHeader::serialization(ser::SDataStream& stream, ser::Action action) {
    auto result{stream.bind(network_magic, action)};
    if (not result.has_error()) result = stream.bind(command, action);
    if (not result.has_error()) result = stream.bind(payload_length, action);
    if (not result.has_error()) result = stream.bind(payload_checksum, action);
    if (not result.has_error() and action == ser::Action::kDeserialize) result = validate();
    return result;
}

outcome::result<void> MessageHeader::validate() noexcept {
    if (payload_length > kMaxProtocolMessageLength) return Error::kMessageHeaderOversizedPayload;

    // Check the command string is made of printable characters
    // eventually right padded to 12 bytes with NUL (0x00) characters.
    bool null_terminator_matched{false};
    size_t got_command_len{0};
    for (const auto chr : command) {
        if (null_terminator_matched && chr != 0) return Error::kMessageHeaderMalformedCommand;
        if (chr == 0) {
            null_terminator_matched = true;
            continue;
        }
        if (chr < 32U || chr > 126U) return Error::kMessageHeaderMalformedCommand;
        ++got_command_len;
    }
    if (got_command_len == 0U) return Error::kMessageHeaderEmptyCommand;

    // Identify the command amongst the known ones
    int definition_id{0};
    for (const auto& msg_def : kMessageDefinitions) {
        if (const auto def_command_len{strnlen_s(msg_def.command, command.size())};
            got_command_len == def_command_len && memcmp(msg_def.command, command.data(), def_command_len) == 0) {
            message_type_ = static_cast<MessageType>(definition_id);
            break;
        }
        ++definition_id;
    }

    if (message_type_ == MessageType::kMissingOrUnknown) return Error::kMessageHeaderIllegalCommand;

    const auto& message_definition{get_definition()};
    if (payload_length < message_definition.min_payload_length.value_or(0U)) {
        return Error::kMessageHeaderUndersizedPayload;
    }
    if (payload_length > message_definition.max_payload_length.value_or(kMaxProtocolMessageLength)) {
        return Error::kMessageHeaderOversizedPayload;
    }

    if (payload_length == 0) /* Hash of empty payload is already known */
    {
        auto empty_payload_hash{crypto::Hash256::kEmptyHash()};
        if (memcmp(payload_checksum.data(), empty_payload_hash.data(), payload_checksum.size()) not_eq 0)
            return Error::kMessageHeaderInvalidChecksum;
    }
    return outcome::success();
}

const MessageDefinition& MessageHeader::get_definition() const noexcept {
    return kMessageDefinitions[static_cast<decltype(kMessageDefinitions)::size_type>(message_type_)];
}

outcome::result<void> Message::validate() noexcept {
    if (ser_stream_.size() < kMessageHeaderLength) return Error::kMessageHeaderIncomplete;
    if (ser_stream_.size() > kMaxProtocolMessageLength) return Error::kMessageHeaderOversizedPayload;

    const auto& message_definition(header_.get_definition());
    if (message_definition.message_type == MessageType::kMissingOrUnknown) return Error::kMessageHeaderIllegalCommand;

    if (ser_stream_.size() < kMessageHeaderLength + header_.payload_length) return Error::kMessageBodyIncomplete;

    // TODO: is this even possible ?
    // if (ser_stream_.size() > kMessageHeaderLength + header_.payload_length) return kMessageMismatchingPayloadLength;

    // From here on ensure we return to the beginning of the payload
    const auto data_to_payload{gsl::finally([this] { ser_stream_.seekg(kMessageHeaderLength); })};

    // Validate payload : length and checksum
    ser_stream_.seekg(kMessageHeaderLength);  // Important : skip the header !!!
    if (auto result = validate_checksum(); result.has_error()) {
        return result.error();
    }
    if (!message_definition.is_vectorized) return outcome::success();

    // For specific messages the vectorized data size can be known in advance
    // e.g. inventory messages are made of 36 bytes elements hence, after the initial
    // read of the vector size the payload size can be checked against the expected size
    ser_stream_.seekg(kMessageHeaderLength);

    // Message `getheaders` payload does not start with the number of items
    // rather with version. We need to skip it (4 bytes)
    if (message_definition.message_type == MessageType::kGetHeaders) {
        ser_stream_.ignore(4);
    }

    auto expected_vector_size{ser::read_compact(ser_stream_)};
    if (expected_vector_size.has_error()) return expected_vector_size.error();
    if (expected_vector_size.value() == 0U) return Error::kMessagePayloadEmptyVector;  // MUST have at least 1 element
    if (expected_vector_size.value() > message_definition.max_vector_items.value_or(UINT32_MAX))
        return Error::kMessagePayloadOversizedVector;
    if (message_definition.vector_item_size.has_value()) {
        // Message `getheaders` has an extra item of 32 bytes (the stop hash)
        const uint64_t extra_item{message_definition.message_type == MessageType::kGetHeaders ? 1U : 0U};
        const auto expected_vector_data_size{(expected_vector_size.value() + extra_item) *
                                             *message_definition.vector_item_size};

        if (ser_stream_.avail() not_eq expected_vector_data_size) return Error::kMessagePayloadMismatchesVectorSize;
        // Look for duplicates
        const auto payload_view{ser_stream_.read()};
        ASSERT_POST(payload_view and "Must have a valid payload view");
        if (const auto duplicate_count{count_duplicate_data_chunks(
                payload_view.value(), *message_definition.vector_item_size, 1 /* one is enough */)};
            duplicate_count > 0) {
            return Error::kMessagePayloadDuplicateVectorItems;
        }
    }

    return outcome::success();
}

outcome::result<void> Message::parse(ByteView& input_data, ByteView network_magic) {
    outcome::result<void> result{outcome::success()};
    while (not result.has_error() and not input_data.empty()) {
        const bool header_mode(ser_stream_.tellg() < kMessageHeaderLength);
        auto bytes_to_read(header_mode ? kMessageHeaderLength - ser_stream_.avail()
                                       : header_.payload_length - ser_stream_.avail());
        bytes_to_read = std::min(bytes_to_read, input_data.size());
        if (result = ser_stream_.write(input_data.substr(0, bytes_to_read)); result.has_error()) return result.error();
        input_data.remove_prefix(bytes_to_read);

        if (not header_mode) {
            result = (ser_stream_.avail() < header_.payload_length) ? Error::kMessageBodyIncomplete : validate();
            break;  // We are done with this message
        }

        if (ser_stream_.avail() < kMessageHeaderLength) return Error::kMessageHeaderIncomplete;

        if (result = header_.deserialize(ser_stream_); result.has_error()) return result.error();
        ASSERT_PRE(network_magic.size() == header_.network_magic.size());
        if (memcmp(header_.network_magic.data(), network_magic.data(), network_magic.size()) != 0) {
            return Error::kMessageHeaderInvalidMagic;
        }

        const auto& message_definition{header_.get_definition()};
        if (message_definition.min_protocol_version.has_value() &&
            ser_stream_.get_version() < *message_definition.min_protocol_version) {
            return Error::kUnsupportedMessageTypeForProtocolVersion;
        }
        if (message_definition.max_protocol_version.has_value() &&
            ser_stream_.get_version() > *message_definition.max_protocol_version) {
            return Error::kDeprecatedMessageTypeForProtocolVersion;
        }

        if (result = header_.validate(); result.has_error()) return result.error();
        if (header_.payload_length == 0) {
            // Hash of empty payload is already known
            auto empty_payload_hash{crypto::Hash256::kEmptyHash()};
            if (memcmp(header_.payload_checksum.data(), empty_payload_hash.data(),
                       header_.payload_checksum.size()) not_eq 0) {
                return Error::kDeprecatedMessageTypeForProtocolVersion;
            }
        }
    }

    return result;
}

outcome::result<void> Message::validate_checksum() noexcept {
    const auto current_pos{ser_stream_.tellg()};
    if (ser_stream_.seekg(kMessageHeaderLength) != kMessageHeaderLength) return Error::kMessageHeaderIncomplete;
    const auto data_to_payload{gsl::finally([this, current_pos] { std::ignore = ser_stream_.seekg(current_pos); })};

    const auto payload_view{ser_stream_.read()};
    if (payload_view.has_error()) return payload_view.error();

    crypto::Hash256 payload_digest(payload_view.value());
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), header_.payload_checksum.data(), header_.payload_checksum.size()) not_eq 0) {
        return Error::kMessageHeaderInvalidChecksum;
    }
    return outcome::success();
}

void Message::set_version(int version) noexcept { ser_stream_.set_version(version); }

int Message::get_version() const noexcept { return ser_stream_.get_version(); }

outcome::result<void> Message::push(const MessageType message_type, MessagePayload& payload, ByteView magic) noexcept {
    using enum net::Error;

    if (not header_.pristine()) return kMessagePushNotPermitted;  // Can't push twice
    if (message_type == MessageType::kMissingOrUnknown) return kMessageUnknownCommand;
    if (magic.size() not_eq header_.network_magic.size()) return kMessageHeaderInvalidMagic;
    header_.set_type(message_type);
    std::memcpy(header_.network_magic.data(), magic.data(), header_.network_magic.size());

    ser_stream_.clear();
    auto result{header_.serialize(ser_stream_)};
    if (result.has_error()) return result.error();
    ASSERT(ser_stream_.size() == kMessageHeaderLength);

    if (result = payload.serialize(ser_stream_); result.has_error()) return result.error();
    header_.payload_length = static_cast<uint32_t>(ser_stream_.size() - kMessageHeaderLength);

    // Compute the checksum
    ser_stream_.seekg(kMessageHeaderLength);  // Move at the beginning of the payload
    const auto payload_view{ser_stream_.read()};
    if (!payload_view) return payload_view.error();
    crypto::Hash256 payload_digest(payload_view.value());
    auto payload_hash{payload_digest.finalize()};
    std::memcpy(header_.payload_checksum.data(), payload_hash.data(), header_.payload_checksum.size());

    // Now copy the lazily computed size and checksum into the datastream
    memcpy(&ser_stream_[16], &header_.payload_length, sizeof(header_.payload_length));
    memcpy(&ser_stream_[20], &header_.payload_checksum, sizeof(header_.payload_checksum));

    return validate();  // Ensure the message is valid also when we push it
}
}  // namespace zenpp::net
