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

namespace zenpp::abi {

void NetMessageHeader::reset() noexcept {
    network_magic.fill('0');
    command.fill('0');
    payload_length = 0;
    payload_checksum.fill('0');
    message_type_ = NetMessageType::kMissingOrUnknown;
}

bool NetMessageHeader::pristine() const noexcept {
    return std::ranges::all_of(network_magic, [](const auto ubyte) { return ubyte == 0U; }) &&
           std::ranges::all_of(command, [](const auto ubyte) { return ubyte == 0U; }) &&
           std::ranges::all_of(payload_checksum, [](const auto ubyte) { return ubyte == 0U; }) &&
           message_type_ == NetMessageType::kMissingOrUnknown && payload_length == 0;
}

void NetMessageHeader::set_type(const NetMessageType type) noexcept {
    if (!pristine()) return;
    const auto& message_definition{kMessageDefinitions[static_cast<size_t>(type)]};
    const auto command_bytes{strnlen_s(message_definition.command, command.size())};
    memcpy(command.data(), message_definition.command, command_bytes);
    message_type_ = type;
}

serialization::Error NetMessageHeader::serialization(serialization::SDataStream& stream, serialization::Action action) {
    using namespace serialization;
    using enum Error;
    Error err{Error::kSuccess};
    if (!err) err = stream.bind(network_magic, action);
    if (!err) err = stream.bind(command, action);
    if (!err) err = stream.bind(payload_length, action);
    if (!err) err = stream.bind(payload_checksum, action);
    if (!err && action == Action::kDeserialize) err = validate();
    return err;
}

serialization::Error NetMessageHeader::validate() noexcept {
    using namespace serialization;
    using enum Error;
    if (payload_length > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    // Check the command string is made of printable characters
    // eventually right padded to 12 bytes with NUL (0x00) characters.
    bool null_terminator_matched{false};
    size_t got_command_len{0};
    for (const auto chr : command) {
        if (null_terminator_matched && chr != 0) return kMessageHeaderMalformedCommand;
        if (chr == 0) {
            null_terminator_matched = true;
            continue;
        }
        if (chr < 32U || chr > 126U) return kMessageHeaderMalformedCommand;
        ++got_command_len;
    }
    if (got_command_len == 0U) return kMessageHeaderEmptyCommand;

    // Identify the command amongst the known ones
    int definition_id{0};
    for (const auto& msg_def : kMessageDefinitions) {
        if (const auto def_command_len{strnlen_s(msg_def.command, command.size())};
            got_command_len == def_command_len && memcmp(msg_def.command, command.data(), def_command_len) == 0) {
            message_type_ = static_cast<NetMessageType>(definition_id);
            break;
        }
        ++definition_id;
    }

    if (message_type_ == NetMessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;

    const auto& message_definition{get_definition()};
    if (message_definition.min_payload_length.has_value() && payload_length < *message_definition.min_payload_length) {
        return kMessageHeaderUndersizedPayload;
    }
    if (message_definition.max_payload_length.has_value() && payload_length > *message_definition.max_payload_length) {
        return kMessageHeaderOversizedPayload;
    }

    if (payload_length == 0) /* Hash of empty payload is already known */
    {
        auto empty_payload_hash{crypto::Hash256::kEmptyHash()};
        if (memcmp(payload_checksum.data(), empty_payload_hash.data(), payload_checksum.size()) != 0)
            return kMessageHeaderInvalidChecksum;
    }

    return kSuccess;
}

const MessageDefinition& NetMessageHeader::get_definition() const noexcept {
    return kMessageDefinitions[static_cast<decltype(kMessageDefinitions)::size_type>(message_type_)];
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-function-cognitive-complexity"
serialization::Error NetMessage::validate() noexcept {
    using enum serialization::Error;

    if (ser_stream_.size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (ser_stream_.size() > kMaxProtocolMessageLength) return kMessageHeaderOversizedPayload;

    const auto& message_definition(header_.get_definition());
    if (message_definition.message_type == NetMessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;

    if (ser_stream_.size() < kMessageHeaderLength) return kMessageHeaderIncomplete;
    if (ser_stream_.size() < kMessageHeaderLength + header_.payload_length) return kMessageBodyIncomplete;
    if (ser_stream_.size() > kMessageHeaderLength + header_.payload_length) return kMessageMismatchingPayloadLength;

    // From here on ensure we return to the beginning of the payload
    const auto data_to_payload{gsl::finally([this] { ser_stream_.seekg(kMessageHeaderLength); })};

    // Validate payload : length and checksum
    ser_stream_.seekg(kMessageHeaderLength);  // Important : skip the header !!!
    if (auto error{validate_checksum()}; error != kSuccess) return error;
    if (!message_definition.is_vectorized) return kSuccess;

    // For specific messages the vectorized data size can be known in advance
    // e.g. inventory messages are made of 36 bytes elements hence, after the initial
    // read of the vector size the payload size can be checked against the expected size
    ser_stream_.seekg(kMessageHeaderLength);

    // Message `getheaders` payload does not start with the number of items
    // rather with version. We need to skip it (4 bytes)
    if (message_definition.message_type == NetMessageType::kGetHeaders) {
        ser_stream_.ignore(4);
    }

    const auto expected_vector_size{serialization::read_compact(ser_stream_)};
    if (!expected_vector_size) return expected_vector_size.error();
    if (*expected_vector_size == 0U) return kMessagePayloadEmptyVector;  // MUST have at least 1 element
    if (*expected_vector_size > message_definition.max_vector_items.value_or(UINT32_MAX))
        return kMessagePayloadOversizedVector;
    if (message_definition.vector_item_size.has_value()) {
        // Message `getheaders` has an extra item of 32 bytes (the stop hash)
        const uint64_t extra_item{message_definition.message_type == NetMessageType::kGetHeaders ? 1U : 0U};
        const auto expected_vector_data_size{(*expected_vector_size + extra_item) *
                                             *message_definition.vector_item_size};

        if (ser_stream_.avail() != expected_vector_data_size) return kMessagePayloadMismatchesVectorSize;
        // Look for duplicates
        const auto payload_view{ser_stream_.read()};
        ASSERT(payload_view);
        if (const auto duplicate_count{count_duplicate_data_chunks(*payload_view, *message_definition.vector_item_size,
                                                                   1 /* one is enough */)};
            duplicate_count > 0) {
            return kMessagePayloadDuplicateVectorItems;
        }
    }

    return kSuccess;
}
#pragma clang diagnostic pop

serialization::Error NetMessage::parse(ByteView& input_data, ByteView network_magic) noexcept {
    using namespace serialization;
    using enum Error;

    Error ret{kSuccess};
    while (!ret && !input_data.empty()) {
        const bool header_mode(ser_stream_.tellg() < kMessageHeaderLength);
        auto bytes_to_read(header_mode ? kMessageHeaderLength - ser_stream_.avail()
                                       : header_.payload_length - ser_stream_.avail());
        bytes_to_read = std::min(bytes_to_read, input_data.size());
        ser_stream_.write(input_data.substr(0, bytes_to_read));
        input_data.remove_prefix(bytes_to_read);

        if (!header_mode) {
            ret = (ser_stream_.avail() < header_.payload_length) ? kMessageBodyIncomplete : validate();
            break;  // We are done with this message
        }

        if (ser_stream_.avail() < kMessageHeaderLength) return kMessageHeaderIncomplete;

        ret = header_.deserialize(ser_stream_);
        if (ret != kSuccess) return ret;

        REQUIRES(network_magic.size() == header_.network_magic.size());
        if (memcmp(header_.network_magic.data(), network_magic.data(), network_magic.size()) != 0) {
            return kMessageHeaderMagicMismatch;
        }

        const auto& message_definition{header_.get_definition()};
        if (message_definition.min_protocol_version.has_value() &&
            ser_stream_.get_version() < *message_definition.min_protocol_version) {
            return kUnsupportedMessageTypeForProtocolVersion;
        }
        if (message_definition.max_protocol_version.has_value() &&
            ser_stream_.get_version() > *message_definition.max_protocol_version) {
            return kDeprecatedMessageTypeForProtocolVersion;
        }

        ret = header_.validate();
        if (ret != kSuccess) return ret;

        if (header_.payload_length == 0) ret = validate_checksum();  // No payload to read
    }

    return ret;
}

serialization::Error NetMessage::validate_checksum() noexcept {
    using enum serialization::Error;
    const auto current_pos{ser_stream_.tellg()};
    if (ser_stream_.seekg(kMessageHeaderLength) != kMessageHeaderLength) return kMessageHeaderIncomplete;
    const auto data_to_payload{gsl::finally([this, current_pos] { std::ignore = ser_stream_.seekg(current_pos); })};

    const auto payload_view{ser_stream_.read()};
    if (!payload_view) return payload_view.error();

    serialization::Error ret{kSuccess};
    crypto::Hash256 payload_digest(*payload_view);
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), header_.payload_checksum.data(), header_.payload_checksum.size()) != 0) {
        ret = kMessageHeaderInvalidChecksum;
    }
    return ret;
}

void NetMessage::set_version(int version) noexcept { ser_stream_.set_version(version); }

int NetMessage::get_version() const noexcept { return ser_stream_.get_version(); }

serialization::Error NetMessage::push(const NetMessageType message_type, NetMessagePayload& payload,
                                      ByteView magic) noexcept {
    using namespace serialization;
    using enum Error;

    if (message_type == NetMessageType::kMissingOrUnknown) return kMessageHeaderUnknownCommand;
    if (magic.size() != header_.network_magic.size()) return kMessageHeaderMagicMismatch;
    if (!header_.pristine()) return kInvalidMessageState;
    header_.set_type(message_type);
    std::memcpy(header_.network_magic.data(), magic.data(), header_.network_magic.size());

    ser_stream_.clear();
    auto err{header_.serialize(ser_stream_)};
    if (!!err) return err;
    ASSERT(ser_stream_.size() == kMessageHeaderLength);

    err = payload.serialize(ser_stream_);
    if (!!err) return err;

    header_.payload_length = static_cast<uint32_t>(ser_stream_.size() - kMessageHeaderLength);

    // Compute the checksum
    ser_stream_.seekg(kMessageHeaderLength);  // Move at the beginning of the payload
    const auto payload_view{ser_stream_.read()};
    if (!payload_view) return payload_view.error();
    crypto::Hash256 payload_digest(*payload_view);
    auto payload_hash{payload_digest.finalize()};
    std::memcpy(header_.payload_checksum.data(), payload_hash.data(), header_.payload_checksum.size());

    // Now copy the lazily computed size and checksum into the datastream
    memcpy(&ser_stream_[16], &header_.payload_length, sizeof(header_.payload_length));
    memcpy(&ser_stream_[20], &header_.payload_checksum, sizeof(header_.payload_checksum));

    return validate();  // Ensure the message is valid also when we push it
}
}  // namespace zenpp::abi
