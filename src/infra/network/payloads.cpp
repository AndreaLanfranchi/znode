/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "payloads.hpp"

#include <absl/strings/str_cat.h.>

#include <core/common/assert.hpp>

namespace zenpp::net {

using namespace ser;

std::shared_ptr<MessagePayload> MessagePayload::from_type(MessageType type) {
    switch (type) {
        using enum MessageType;
        case kVersion:
            return std::make_shared<MsgVersionPayload>();
        case kPing:
            return std::make_shared<MsgPingPayload>();
        case kPong:
            return std::make_shared<MsgPongPayload>();
        case kGetHeaders:
            return std::make_shared<MsgGetHeadersPayload>();
        case kAddr:
            return std::make_shared<MsgAddrPayload>();
        case kReject:
            return std::make_shared<MsgRejectPayload>();

            /* Following do not have a payload */

        case kVerAck:
        case kMemPool:
        case kMissingOrUnknown:
            return std::make_shared<MsgNullPayload>(type);
        default:
            return nullptr;
    }
}

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
nlohmann::json MsgVersionPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["protocol_version"] = protocol_version_;

    nlohmann::json services_array(nlohmann::json::value_t::array);
    for (auto& item : magic_enum::enum_values<NodeServicesType>()) {
        const auto item_value = static_cast<uint64_t>(item);
        if (item_value == 0 or item_value == static_cast<uint64_t>(NodeServicesType::kNodeNetworkAll)) continue;
        if (services_ bitand item_value) {
            services_array.push_back(std::string(magic_enum::enum_name(item)).substr(1 /* skip k */));
        }
    }
    ret["services"] = std::move(services_array);
    ret["timestamp"] = timestamp_;
    ret["recipient_service"] = recipient_service_.to_json();
    ret["sender_service"] = sender_service_.to_json();
    ret["nonce"] = nonce_;
    ret["user_agent"] = user_agent_;
    ret["last_block_height"] = last_block_height_;
    ret["relay"] = relay_;
    return ret;
}

outcome::result<void> MsgPingPayload::serialization(ser::SDataStream& stream, ser::Action action) {
    return stream.bind(nonce_, action);
}

nlohmann::json MsgPingPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["nonce"] = nonce_;
    return ret;
}

outcome::result<void> MsgPongPayload::serialization(ser::SDataStream& stream, ser::Action action) {
    return stream.bind(nonce_, action);
}

nlohmann::json MsgPongPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["nonce"] = nonce_;
    return ret;
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
nlohmann::json MsgGetHeadersPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["protocol_version"] = protocol_version_;
    nlohmann::json hashes(nlohmann::json::value_t::array);
    for (auto& item : block_locator_hashes_) {
        hashes.push_back(item.to_string());
    }
    ret["hashes"] = std::move(hashes);
    ret["hash_stop"] = hash_stop_.to_string();
    return ret;
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
nlohmann::json MsgAddrPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    nlohmann::json identifiers(nlohmann::json::value_t::array);
    for (auto& item : identifiers_) {
        identifiers.push_back(item.to_json());
    }
    ret["identifiers"] = std::move(identifiers);
    return ret;
}

outcome::result<void> MsgRejectPayload::serialization(SDataStream& stream, ser::Action action) {
    outcome::result<void> result = outcome::success();
    if (action == Action::kSerialize) {
        if (not is_known_command(rejected_command_)) return Error::kUnknownRejectedCommand;
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
        if (not result.has_error() and not is_known_command(rejected_command_)) return Error::kUnknownRejectedCommand;
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
    }

    return result;
}
nlohmann::json MsgRejectPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["rejected_command"] = rejected_command_;
    ret["rejection_code"] = magic_enum::enum_name(rejection_code_);
    ret["reason"] = reason_;
    if (extra_data_.has_value()) {
        ret["extra_data"] = extra_data_.value().to_string();
    }
    return ret;
}
}  // namespace zenpp::net
