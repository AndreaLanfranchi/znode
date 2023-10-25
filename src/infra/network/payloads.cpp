/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "payloads.hpp"

#include <infra/network/messages.hpp>

namespace zenpp::net {

using namespace ser;

outcome::result<void> MsgVersionPayload::serialization(SDataStream& stream, Action action) {
    auto result{stream.bind(protocol_version_, action)};
    if (not result.has_error()) result = stream.bind(services_, action);
    if (not result.has_error()) result = stream.bind(timestamp_, action);
    if (not result.has_error()) result = stream.bind(recipient_service_, action);
    if (not result.has_error()) result = stream.bind(sender_service_, action);
    if (not result.has_error()) result = stream.bind(nonce_, action);
    if (not result.has_error()) result = stream.bind(user_agent_, action);
    if (not result.has_error()) result = stream.bind(last_block_height_, action);
    if (not result.has_error()) result = stream.bind(relay_, action);
    if (action == Action::kDeserialize) {
        if (timestamp_ < 0) return Error::kMessagePayloadInvalidTimestamp;
        if (last_block_height_ < 0) return Error::kMessagePayloadInvalidLastBlockHeight;
    }
    return result;
}

outcome::result<void> MsgPingPayload::serialization(ser::SDataStream& stream, ser::Action action) {
    return stream.bind(nonce_, action);
}

outcome::result<void> MsgPongPayload::serialization(ser::SDataStream& stream, ser::Action action) {
    return stream.bind(nonce_, action);
}

outcome::result<void> MsgGetHeadersPayload::serialization(SDataStream& stream, ser::Action action) {
    protocol_version_ =
        (action == Action::kSerialize) ? static_cast<decltype(protocol_version_)>(stream.get_version()) : 0U;
    auto result = stream.bind(protocol_version_, action);
    if (not result.has_error()) {
        if (action == Action::kSerialize) {
            const auto vector_size = block_locator_hashes_.size();
            if (vector_size == 0U) return Error::kMessagePayloadEmptyVector;
            if (vector_size > kMaxGetHeadersItems) return Error::kMessagePayloadOversizedVector;
            if (result = write_compact(stream, vector_size); result.has_error()) return result.error();
            for (auto& item : block_locator_hashes_) {
                if (result = item.serialize(stream); result.has_error()) break;
            }
            if (not result.has_error()) result = hash_stop_.serialize(stream);
        } else {
            const auto expected_vector_size{read_compact(stream)};
            if (expected_vector_size.has_error()) return expected_vector_size.error();
            if (expected_vector_size.value() == 0U) return Error::kMessagePayloadEmptyVector;
            if (expected_vector_size.value() > kMaxHeadersItems) return Error::kMessagePayloadOversizedVector;
            block_locator_hashes_.resize(expected_vector_size.value());
            for (auto& item : block_locator_hashes_) {
                if (result = item.deserialize(stream); result.has_error()) break;
            }
            if (not result.has_error()) result = hash_stop_.deserialize(stream);
        }
    }
    return result;
}

outcome::result<void> MsgAddrPayload::serialization(SDataStream& stream, ser::Action action) {
    if (action == Action::kSerialize) {
        const auto vector_size = identifiers_.size();
        if (vector_size == 0U) return Error::kMessagePayloadEmptyVector;
        if (vector_size > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        if (auto result = write_compact(stream, vector_size); result.has_error()) return result.error();
        for (auto& item : identifiers_) {
            if (auto result{item.serialize(stream)}; result.has_error()) return result.error();
        }
    } else {
        const auto expected_vector_size = read_compact(stream);
        if (expected_vector_size.has_error()) return expected_vector_size.error();
        if (expected_vector_size.value() == 0U) return Error::kMessagePayloadEmptyVector;
        if (expected_vector_size.value() > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        identifiers_.resize(expected_vector_size.value());
        for (auto& item : identifiers_) {
            if (auto result = item.deserialize(stream); result.has_error()) return result.error();
        }
    }
    return outcome::success();
}

outcome::result<void> MsgRejectPayload::serialization(SDataStream& stream, ser::Action action) {
    outcome::result<void> result = outcome::success();
    if (action == Action::kSerialize) {
        if (rejected_command_.empty() or rejected_command_.size() > 12) {
            return Error::kMessageHeaderIllegalCommand;
        }
        if (reason_.size() > 256) {
            return ser::Error::kStringTooBig;
        }
        result = stream.bind(rejected_command_, action);
        if (not result.has_error()) {
            auto code = static_cast<int8_t>(rejection_code_);
            result = stream.bind(code, action);
        }
        if (not result.has_error()) result = stream.bind(reason_, action);
        if (not result.has_error() and extra_data_.has_value()) result = stream.bind(extra_data_.value(), action);

    } else {
        result = stream.bind(rejected_command_, action);
        if (not result.has_error()) {
            int8_t code{0};
            result = stream.bind(code, action);
            if (not result.has_error()) {
                auto enumerator = magic_enum::enum_cast<RejectionCode>(code);
                if (not enumerator.has_value()) {
                    return ser::Error::kInvalidRejectionCode;
                }
            }
        }
        if (not result.has_error()) result = stream.bind(reason_, action);
        if (not result.has_error()) {
            if (reason_.size() > 256) {
                return ser::Error::kStringTooBig;
            }
            if (stream.avail() >= h256::size()) {
                h256 extra_data_value{};
                result = stream.bind(extra_data_value, action);
                if (not result.has_error()) {
                    extra_data_.emplace(std::move(extra_data_value));
                }
            }
        }
        if (not result.has_error() and stream.avail() > 0) {
            return ser::Error::kInputTooLarge;
        }
    }

    return result;
}
}  // namespace zenpp::net
