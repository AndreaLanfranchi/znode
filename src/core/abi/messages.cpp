/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/abi/messages.hpp>

namespace zenpp::abi {

using namespace serialization;

Error MsgVersionPayload::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (!ret) ret = stream.bind(protocol_version_, action);
    if (!ret) ret = stream.bind(services_, action);
    if (!ret) ret = stream.bind(timestamp_, action);
    if (!ret) ret = stream.bind(addr_recv_, action);
    if (!ret) ret = stream.bind(addr_from_, action);
    if (!ret) ret = stream.bind(nonce_, action);
    if (!ret) ret = stream.bind(user_agent_, action);
    if (!ret) ret = stream.bind(last_block_height_, action);
    if (!ret) ret = stream.bind(relay_, action);
    return ret;
}

Error MsgPingPongPayload::serialization(serialization::SDataStream& stream, serialization::Action action) {
    return stream.bind(nonce_, action);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-function-cognitive-complexity"
Error MsgGetHeadersPayload::serialization(SDataStream& stream, serialization::Action action) {
    using enum Error;
    Error ret{kSuccess};
    protocol_version_ =
        (action == Action::kSerialize) ? static_cast<decltype(protocol_version_)>(stream.get_version()) : 0U;
    if (!ret) ret = stream.bind(protocol_version_, action);
    if (!ret) {
        if (action == Action::kSerialize) {
            const auto vector_size = block_locator_hashes_.size();
            if (vector_size == 0U) return Error::kMessagePayloadEmptyVector;
            if (vector_size > 2000U) return Error::kMessagePayloadOversizedVector;
            write_compact(stream, vector_size);
            for (auto& item : block_locator_hashes_) {
                if (ret != Error::kSuccess) break;
                ret = item.serialize(stream);
            }
            if (!ret) ret = hash_stop_.serialize(stream);
        } else {
            const auto expected_vector_size = read_compact(stream);
            if (!expected_vector_size) return expected_vector_size.error();
            if (*expected_vector_size == 0U) return Error::kMessagePayloadEmptyVector;
            if (*expected_vector_size > 2000U) return Error::kMessagePayloadOversizedVector;
            block_locator_hashes_.resize(*expected_vector_size);
            for (auto& item : block_locator_hashes_) {
                if (ret != Error::kSuccess) break;
                ret = item.deserialize(stream);
            }
            if (!ret) ret = hash_stop_.deserialize(stream);
        }
    }

    return ret;
}
#pragma clang diagnostic pop

serialization::Error MsgAddrPayload::serialization(SDataStream& stream, serialization::Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (action == Action::kSerialize) {
        const auto vector_size = identifiers_.size();
        if (vector_size == 0U) return Error::kMessagePayloadEmptyVector;
        if (vector_size > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        write_compact(stream, vector_size);
        for (auto& item : identifiers_) {
            if (ret != Error::kSuccess) break;
            ret = item.serialize(stream);
        }
    } else {
        const auto vector_size = read_compact(stream);
        if (!vector_size) return vector_size.error();
        if (vector_size.value() == 0) return Error::kMessagePayloadEmptyVector;
        if (vector_size.value() > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        identifiers_.resize(vector_size.value());
        for (auto& item : identifiers_) {
            if (ret != Error::kSuccess) break;
            ret = item.deserialize(stream);
        }
    }

    return ret;
}
}  // namespace zenpp::abi
