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

#include <boost/algorithm/clamp.hpp>
#include <gsl/gsl_util>

namespace zenpp::net {

void MessageHeader::reset() noexcept {
    network_magic.fill(0);
    command.fill(0);
    payload_length = 0;
    payload_checksum.fill(0);
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
    return result;
}

outcome::result<void> MessageHeader::validate(int protocol_version, const ByteView magic) noexcept {
    // Check the magic number is correct
    if (magic.size() not_eq network_magic.size()) return Error::kMessageHeaderInvalidMagic;
    if (memcmp(network_magic.data(), magic.data(), magic.size()) not_eq 0) return Error::kMessageHeaderInvalidMagic;

    // Identify the command
    const auto get_command_label = [](const MessageType type, const size_t to_size) -> Bytes {
        std::string label{magic_enum::enum_name(type)};
        label.erase(0, 1);
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return std::tolower(c); });
        Bytes ret{label.begin(), label.end()};
        ret.resize(to_size, 0x0);
        return ret;
    };

    for (const auto enumerator : magic_enum::enum_values<MessageType>()) {
        const auto command_label{get_command_label(enumerator, command.size())};
        ASSERT(command_label.size() == command.size());
        if (memcmp(command_label.data(), command.data(), command.size()) == 0) {
            message_type_ = enumerator;
            break;
        }
    }
    if (message_type_ == MessageType::kMissingOrUnknown) return Error::kMessageHeaderIllegalCommand;

    // Check the payload length is within the allowed range
    if ((payload_length) > kMaxProtocolMessageLength) {
        return Error::kMessageHeaderIllegalPayloadLength;
    }

    // Verify message command is allowed by protocol version
    const auto& message_definition{get_definition()};
    if (message_definition.min_protocol_version.has_value() &&
        protocol_version < *message_definition.min_protocol_version) {
        return Error::kUnsupportedMessageTypeForProtocolVersion;
    }
    if (message_definition.max_protocol_version.has_value() &&
        protocol_version > *message_definition.max_protocol_version) {
        return Error::kDeprecatedMessageTypeForProtocolVersion;
    }

    // Verify payload size falls within the allowed range
    if (boost::algorithm::clamp(static_cast<size_t>(payload_length),
                                message_definition.min_payload_length.value_or(size_t(0U)),
                                message_definition.max_payload_length.value_or(kMaxProtocolMessageLength)) not_eq
        static_cast<size_t>(payload_length)) {
        return Error::kMessageHeaderIllegalPayloadLength;
    }

    // In case of empty payload, the checksum is already known
    if (payload_length == 0) {
        auto empty_payload_hash{crypto::Hash256::kEmptyHash()};
        if (memcmp(payload_checksum.data(), empty_payload_hash.data(), payload_checksum.size()) not_eq 0)
            return Error::kMessageHeaderInvalidChecksum;
    }

    return outcome::success();
}

const MessageDefinition& MessageHeader::get_definition() const noexcept {
    return kMessageDefinitions[static_cast<decltype(kMessageDefinitions)::size_type>(message_type_)];
}

MessageType Message::get_type() const noexcept { return header_.get_type(); }

void Message::reset() noexcept {
    header_.reset();
    ser_stream_.clear();
    header_validated_ = false;
    payload_validated_ = false;
}

outcome::result<void> Message::validate() noexcept {
    if (ser_stream_.size() > kMaxProtocolMessageLength) return Error::kMessageSizeOverflow;
    if (is_complete()) return outcome::success();
    auto result{validate_header()};
    if (result.has_error()) return result.error();
    result = validate_payload();
    ASSERT(ser_stream_.seekg(kMessageHeaderLength) == kMessageHeaderLength);
    return result;
}

outcome::result<void> Message::push(MessagePayload& payload) noexcept {
    using enum net::Error;

    if (not header_.pristine()) return kMessagePushNotPermitted;  // Can't push twice
    if (payload.type() == MessageType::kMissingOrUnknown) return kMessageUnknownCommand;
    header_.set_type(payload.type());
    std::memcpy(header_.network_magic.data(), network_magic_.data(), header_.network_magic.size());

    ser_stream_.clear();
    auto result{header_.serialize(ser_stream_)};
    if (result.has_error()) return result.error();
    ASSERT(ser_stream_.size() == kMessageHeaderLength);

    if (result = payload.serialize(ser_stream_); result.has_error()) return result.error();
    header_.payload_length = static_cast<uint32_t>(ser_stream_.size() - kMessageHeaderLength);

    // Compute the checksum
    ASSERT(ser_stream_.seekg(kMessageHeaderLength) == kMessageHeaderLength);  // Move at the beginning of the payload
    const auto payload_view{ser_stream_.read()};
    if (!payload_view) return payload_view.error();
    crypto::Hash256 payload_digest(payload_view.value());
    auto payload_hash{payload_digest.finalize()};
    std::memcpy(header_.payload_checksum.data(), payload_hash.data(), header_.payload_checksum.size());

    // Now copy the lazily computed size and checksum into the datastream
    memcpy(&ser_stream_[16], &header_.payload_length, sizeof(header_.payload_length));
    memcpy(&ser_stream_[20], &header_.payload_checksum, sizeof(header_.payload_checksum));

    ASSERT(ser_stream_.seekg(0) == 0);  // Move at the beginning of the whole message
    return validate();                  // Ensure the message is valid also when we push it
}

outcome::result<void> Message::write(ByteView& input) {
    outcome::result<void> result{outcome::success()};
    if (input.empty()) {
        if (not header_validated_) return Error::kMessageHeaderIncomplete;
        if (not payload_validated_) return Error::kMessageBodyIncomplete;
        return outcome::success();
    }
    if (is_complete()) return Error::kMessageWriteNotPermitted;  // Can't write twice
    while (not input.empty()) {
        // Grab as many bytes either to have a complete header or a complete message
        const bool header_mode(ser_stream_.tellg() < kMessageHeaderLength);
        auto bytes_to_read(header_mode ? kMessageHeaderLength - ser_stream_.avail()
                                       : header_.payload_length - ser_stream_.avail());
        bytes_to_read = std::min(bytes_to_read, input.size());
        if (result = ser_stream_.write(input.substr(0, bytes_to_read)); result.has_error()) return result.error();
        input.remove_prefix(bytes_to_read);

        // Validate the message
        result = validate();
        if (not result.has_error()) break;

        // If the message is incomplete, we need to continue reading
        if (result.error() == Error::kMessageHeaderIncomplete or result.error() == Error::kMessageBodyIncomplete) {
            continue;
        }

        // Any other error is fatal - higher code should discard pending data
        break;
    }

    return result;
}

outcome::result<void> Message::validate_header() noexcept {
    if (header_validated_) return outcome::success();
    if (ser_stream_.size() < kMessageHeaderLength) return Error::kMessageHeaderIncomplete;
    auto result{header_.deserialize(ser_stream_)};
    if (result.has_error()) return result.error();
    result = header().validate(ser_stream_.get_version(), network_magic_);
    if (not result.has_error()) {
        header_validated_ = true;
        payload_validated_ = header_.payload_length == 0;  // No need to check payload if empty
    }
    return result;
}

outcome::result<void> Message::validate_payload() noexcept {
    if (payload_validated_) return outcome::success();
    if (not header_validated_) return Error::kMessageHeaderIncomplete;
    const bool payload_incomplete{ser_stream_.avail() not_eq header_.payload_length};
    const auto reset_position{gsl::finally([this] { ser_stream_.seekg(kMessageHeaderLength); })};

    // Validate payload in case of vectorized data
    const auto& message_definition(header_.get_definition());
    if (message_definition.is_vectorized) {
        auto result{validate_payload_vector(message_definition)};
        if (result.has_error()) return result.error();
        if (not payload_incomplete and message_definition.vector_item_size.has_value()) {
            // Look for duplicates
            const auto payload_view{ser_stream_.read()};
            ASSERT_POST(payload_view and "Must have a valid payload view");
            if (const auto duplicate_count{count_duplicate_data_chunks(
                    payload_view.value(), *message_definition.vector_item_size, 1 /* one is enough */)};
                duplicate_count > 0) {
                return Error::kMessagePayloadDuplicateVectorItems;
            }
        }
    }

    if (payload_incomplete) return Error::kMessageBodyIncomplete;

    // Validate payload's checksum (if any)
    ASSERT_PRE(ser_stream_.seekg(kMessageHeaderLength) == kMessageHeaderLength);
    if (auto checksum_validation{validate_payload_checksum()}; checksum_validation.has_error()) {
        return checksum_validation.error();
    }

    // Payload is formally valid
    // Syntactically valid payload is verified during deserialize
    payload_validated_ = true;
    return outcome::success();
}

outcome::result<void> Message::validate_payload_vector(const MessageDefinition& message_definition) noexcept {
    // Determine if we have enough data to read the vector size
    // Particular cases are:
    // - `getheaders` where the count of item vectors has an offset by 4 bytes
    const size_t offset{message_definition.message_type == MessageType::kGetHeaders ? 4U : 0U};
    const size_t pos{kMessageHeaderLength + offset};
    if (ser_stream_.seekg(pos) not_eq pos or ser_stream_.avail() < 1) return Error::kMessageBodyIncomplete;

    // Read the number of elements defined in the vector
    const auto num_elements{ser::read_compact(ser_stream_)};
    if (num_elements.has_error()) {
        if (num_elements.error() == ser::Error::kReadOverflow) {
            return Error::kMessageBodyIncomplete;
        }
        return num_elements.error();
    }
    if (num_elements.value() == 0U) return Error::kMessagePayloadEmptyVector;
    if (num_elements.value() > message_definition.max_vector_items.value_or(ser::kMaxSerializedCompactSize)) {
        return Error::kMessagePayloadOversizedVector;
    }

    if (message_definition.vector_item_size.has_value()) {
        // Compute the expected size of the vector and compare with the actual payload length declared in the header
        // Message `getheaders` has an extra item of 32 bytes (the stop hash)
        auto expected_vector_data_size{num_elements.value() * message_definition.vector_item_size.value()};
        if (message_definition.message_type == MessageType::kGetHeaders) {
            expected_vector_data_size += message_definition.vector_item_size.value();
        }
        expected_vector_data_size += ser::ser_compact_sizeof(num_elements.value());
        if (header_.payload_length not_eq (expected_vector_data_size + offset)) {
            return Error::kMessagePayloadLengthMismatchesVectorSize;
        }
    }
    return outcome::success();
}

outcome::result<void> Message::validate_payload_checksum() noexcept {
    const auto payload_view{ser_stream_.read()};
    if (payload_view.has_error()) return payload_view.error();
    ASSERT_POST(payload_view.value().size() == header_.payload_length);

    crypto::Hash256 payload_digest(payload_view.value());
    if (auto payload_hash{payload_digest.finalize()};
        memcmp(payload_hash.data(), header_.payload_checksum.data(), header_.payload_checksum.size()) not_eq 0) {
        return Error::kMessageHeaderInvalidChecksum;
    }
    return outcome::success();
}
}  // namespace zenpp::net
