/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/abi/messages.hpp>

namespace zenpp::abi {

using namespace serialization;

Error Version::serialization(SDataStream& stream, Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (!ret) ret = stream.bind(version, action);
    if (!ret) ret = stream.bind(services, action);
    if (!ret) ret = stream.bind(timestamp, action);
    if (!ret) ret = stream.bind(addr_recv, action);
    if (!ret) ret = stream.bind(addr_from, action);
    if (!ret) ret = stream.bind(nonce, action);
    if (!ret) ret = stream.bind(user_agent, action);
    if (!ret) ret = stream.bind(start_height, action);
    if (!ret) ret = stream.bind(relay, action);
    return ret;
}

Error PingPong::serialization(serialization::SDataStream& stream, serialization::Action action) {
    return stream.bind(nonce, action);
}

Error GetHeaders::serialization(SDataStream& stream, serialization::Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (!ret) ret = stream.bind(version, action);
    if (!ret) {
        if (action == Action::kSerialize) {
            const auto vector_size = block_locator_hashes.size();
            if (!vector_size) return Error::kMessagePayloadEmptyVector;
            if (vector_size > 2000) return Error::kMessagePayloadOversizedVector;
            write_compact(stream, vector_size);
            for (auto& item : block_locator_hashes) {
                if (!ret) {
                    ret = item.serialize(stream);
                } else {
                    break;
                }
            }
            if (!ret) ret = hash_stop.serialize(stream);
        } else {
            const auto vector_size = read_compact(stream);
            if (!vector_size) return vector_size.error();
            if (vector_size.value() == 0) return Error::kMessagePayloadEmptyVector;
            if (vector_size.value() > 2000) return Error::kMessagePayloadOversizedVector;
            block_locator_hashes.resize(vector_size.value());
            for (auto& item : block_locator_hashes) {
                if (!ret) {
                    ret = item.deserialize(stream);
                } else {
                    break;
                }
            }
            if (!ret) ret = hash_stop.deserialize(stream);
        }
    }

    return ret;
}
serialization::Error Addr::serialization(SDataStream& stream, serialization::Action action) {
    using enum Error;
    Error ret{kSuccess};
    if (action == Action::kSerialize) {
        const auto vector_size = addresses.size();
        if (!vector_size) return Error::kMessagePayloadEmptyVector;
        if (vector_size > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        write_compact(stream, vector_size);
        for (auto& item : addresses) {
            if (!ret) {
                ret = item.serialize(stream);
            } else {
                break;
            }
        }
    } else {
        const auto vector_size = read_compact(stream);
        if (!vector_size) return vector_size.error();
        if (vector_size.value() == 0) return Error::kMessagePayloadEmptyVector;
        if (vector_size.value() > kMaxAddrItems) return Error::kMessagePayloadOversizedVector;
        addresses.resize(vector_size.value());
        for (auto& item : addresses) {
            if (!ret) {
                ret = item.deserialize(stream);
            } else {
                break;
            }
        }
    }

    return ret;
}
}  // namespace zenpp::abi
