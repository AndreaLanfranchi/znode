/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "payloads.hpp"

#include <random>

#include <absl/strings/str_cat.h.>

#include <core/common/assert.hpp>

namespace zenpp::net {

using namespace ser;

std::shared_ptr<MessagePayload> MessagePayload::from_type(MessageType type) {
    switch (type) {
        using enum MessageType;
        case kVersion:
            return std::make_shared<MsgVersionPayload>();

            /* Same Payload for both Ping and Pong */

        case kPing:
        case kPong:
            return std::make_shared<MsgPingPongPayload>(type);

        case kGetHeaders:
            return std::make_shared<MsgGetHeadersPayload>();
        case kAddr:
            return std::make_shared<MsgAddrPayload>();
        case kInv:
        case kGetData:
            return std::make_shared<MsgInventoryPayload>(type);
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
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);

    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];

    data["protocol_version"] = protocol_version_;
    data["services"] = nlohmann::json(nlohmann::json::value_t::array);
    auto& services_array = data["services"];
    for (auto& item : magic_enum::enum_values<NodeServicesType>()) {
        const auto item_value = static_cast<uint64_t>(item);
        if (item_value == 0 or item_value == static_cast<uint64_t>(NodeServicesType::kNodeNetworkAll)) continue;
        if (services_ bitand item_value) {
            services_array.push_back(std::string(magic_enum::enum_name(item)).substr(1 /* skip k */));
        }
    }

    data["timestamp"] = timestamp_;
    data["recipient_service"] = recipient_service_.to_json();
    data["sender_service"] = sender_service_.to_json();
    data["nonce"] = nonce_;
    data["user_agent"] = user_agent_;
    data["last_block_height"] = last_block_height_;
    data["relay"] = relay_;

    return ret;
}

nlohmann::json MsgPingPongPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);
    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];
    data["nonce"] = nonce_;
    return ret;
}

outcome::result<void> MsgPingPongPayload::serialization(ser::SDataStream& stream, ser::Action action) {
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
nlohmann::json MsgGetHeadersPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);
    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];

    data["protocol_version"] = protocol_version_;
    data["hashes"] = nlohmann::json(nlohmann::json::value_t::array);
    auto& hashes = data["hashes"];
    for (auto& item : block_locator_hashes_) {
        hashes.push_back(item.to_hex(/*reverse=*/true, /*with_prefix=*/true));
    }
    data["hash_stop"] = hash_stop_.to_hex(/*reverse=*/true, /*with_prefix=*/true);
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
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);
    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];
    data["identifiers"] = nlohmann::json(nlohmann::json::value_t::array);
    auto& identifiers = data["identifiers"];
    for (auto& item : identifiers_) {
        identifiers.push_back(item.to_json());
    }
    return ret;
}

void MsgAddrPayload::shuffle() noexcept {
    std::random_device rnd;
    std::mt19937 gen(rnd());
    std::shuffle(identifiers_.begin(), identifiers_.end(), gen);
}

outcome::result<void> MsgInventoryPayload::serialization(SDataStream& stream, ser::Action action) {
    if (action == Action::kSerialize) {
        const auto vector_size = items_.size();
        if (vector_size == 0U) return Error::kMessagePayloadEmptyVector;
        if (vector_size > kMaxInvItems) return Error::kMessagePayloadOversizedVector;
        if (auto result = write_compact(stream, vector_size); result.has_error()) return result.error();
        for (auto& item : items_) {
            if (auto result{item.serialize(stream)}; result.has_error()) return result.error();
        }
    } else {
        const auto expected_vector_size = read_compact(stream);
        if (expected_vector_size.has_error()) return expected_vector_size.error();
        if (expected_vector_size.value() == 0U) return Error::kMessagePayloadEmptyVector;
        if (expected_vector_size.value() > kMaxInvItems) return Error::kMessagePayloadOversizedVector;
        items_.resize(expected_vector_size.value());
        for (auto& item : items_) {
            if (auto result = item.deserialize(stream); result.has_error()) return result.error();
        }
    }
    return outcome::success();
}

nlohmann::json MsgInventoryPayload::to_json() const {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);
    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];
    data["items"] = nlohmann::json(nlohmann::json::value_t::array);
    auto& items = data["items"];
    for (auto& item : items_) {
        items.push_back(item.to_json());
    }
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
    ret["command"] = std::string(magic_enum::enum_name(type())).substr(1);
    ret["data"] = nlohmann::json(nlohmann::json::value_t::object);
    auto& data = ret["data"];
    data["rejected_command"] = rejected_command_;
    data["rejection_code"] = std::string(magic_enum::enum_name(rejection_code_)).substr(1);
    data["reason"] = reason_;
    if (extra_data_.has_value()) {
        data["extra_data"] = extra_data_.value().to_string();
    }
    return ret;
}
}  // namespace zenpp::net
