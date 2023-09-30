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
    if (not result.has_error() and action == ser::Action::kDeserialize) result = validate(stream.get_version());
    return result;
}

outcome::result<void> MessageHeader::validate(int protocol_version) noexcept {
    // Check the payload length is within the allowed range
    if ((payload_length + kMessageHeaderLength) > kMaxProtocolMessageLength)
        return Error::kMessageHeaderIllegalPayloadLength;

    // Identify the command
    const auto get_command_label = [](const MessageType type, const size_t to_size) -> std::string {
        std::string ret{magic_enum::enum_name(type)};
        ret.erase(0, 1);
        std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ret.size() < to_size) ret.append(to_size - ret.size(), 0x0);
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
                                get_definition().min_payload_length.value_or(size_t(0U)),
                                get_definition().max_payload_length.value_or(kMaxProtocolMessageLength)) not_eq
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

std::optional<MessageType> Message::get_type() const noexcept {
    if (not header_validated_) return std::nullopt;
    return header_.get_type();
}

outcome::result<void> Message::validate() noexcept {
    if (ser_stream_.size() > kMaxProtocolMessageLength) return Error::kMessageSizeOverflow;
    if (complete_) return outcome::success();
    if (not header_validated_) {
        if (auto validation_result{validate_header()}; validation_result.has_error()) {
            return validation_result.error();
        }
        header_validated_ = true;
        ASSERT(ser_stream_.seekg(kMessageHeaderLength) == kMessageHeaderLength);
    }
    return validate_payload();
}

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

outcome::result<void> Message::write(ByteView& input, ByteView network_magic) {
    outcome::result<void> result{outcome::success()};
    if (complete_) return Error::kMessageWriteNotPermitted;  // Can't write twice
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
        if (not result.has_error()) {
            complete_ = true;  // This message is fully parsed and validated
            break;
        }

        // If the message is incomplete, we need to continue reading
        if (result.error() == Error::kMessageHeaderIncomplete or result.error() == Error::kMessageBodyIncomplete) {
            result = outcome::success();
            continue;
        }

        // Any other error is fatal - higher code should discard pending data
        break;

        // TODO: Add a check for network magic
        //        if (result = header_.deserialize(ser_stream_); result.has_error()) return result.error();
        //        ASSERT_PRE(network_magic.size() == header_.network_magic.size());
        //        if (memcmp(header_.network_magic.data(), network_magic.data(), network_magic.size()) != 0) {
        //            return Error::kMessageHeaderInvalidMagic;
        //        }
    }

    return result;
}

outcome::result<void> Message::validate_header() noexcept {
    if (header_validated_) return outcome::success();
    if (ser_stream_.size() < kMessageHeaderLength) return Error::kMessageHeaderIncomplete;
    if (auto result{header_.deserialize(ser_stream_)}; result.has_error()) return result.error();
    return header().validate(ser_stream_.get_version());
}

outcome::result<void> Message::validate_payload() noexcept {
    if (not header_validated_) return Error::kMessageHeaderIncomplete;
    if (header_.payload_length == 0) return outcome::success();
    if (ser_stream_.avail() < header_.payload_length) return Error::kMessageBodyIncomplete;

    // Validate payload in case of vectorized data
    const auto& message_definition(header_.get_definition());
    if (not message_definition.is_vectorized) return outcome::success();

    // For specific messages the vectorized data size can be known in advance
    // e.g. inventory messages are made of 36 bytes elements hence, after the initial
    // read of the vector size the payload size can be checked against the expected size
    ser_stream_.seekg(kMessageHeaderLength);
    const auto reset_position{gsl::finally([this] { ser_stream_.seekg(kMessageHeaderLength); })};

    // Message `getheaders` payload does not start with the number of items
    // rather with version. We need to skip it (4 bytes)
    if (message_definition.message_type == MessageType::kGetHeaders) {
        ser_stream_.ignore(4);
    }

    auto expected_vector_size{ser::read_compact(ser_stream_)};
    if (expected_vector_size.has_error()) return expected_vector_size.error();
    if (expected_vector_size.value() == 0U) return Error::kMessagePayloadEmptyVector;  // MUST have at least 1 element
    if (expected_vector_size.value() > message_definition.max_vector_items.value_or(UINT32_MAX)) {
        return Error::kMessagePayloadOversizedVector;
    }
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

    // Validate payload's checksum (if any)
    ser_stream_.seekg(kMessageHeaderLength);
    if (auto checksum_validation{validate_checksum()}; checksum_validation.has_error()) {
        return checksum_validation.error();
    }

    // Payload is formally valid
    // Syntactically valid payload is verified during deserialize
    return outcome::success();
}

outcome::result<void> Message::validate_checksum() noexcept {
    const auto current_pos{ser_stream_.tellg()};
    ASSERT_PRE(current_pos == kMessageHeaderLength);
    const auto reset_to_pos{gsl::finally([this, current_pos] { std::ignore = ser_stream_.seekg(current_pos); })};

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
