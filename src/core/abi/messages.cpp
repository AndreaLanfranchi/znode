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
}  // namespace zenpp::abi