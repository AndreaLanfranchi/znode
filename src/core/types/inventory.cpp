/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "inventory.hpp"

#include <magic_enum.hpp>

namespace znode {

void InventoryItem::reset() {
    type_ = Type::kError;
    identifier_.reset();
}

nlohmann::json InventoryItem::to_json() const noexcept {
    nlohmann::json ret(nlohmann::json::value_t::object);
    ret["type"] = std::string(magic_enum::enum_name(type_).substr(1));
    ret["identifier"] = identifier_.to_hex(/*reverse=*/true, /*with_prefix=*/true);
    return ret;
}

outcome::result<void> InventoryItem::serialization(ser::SDataStream& stream, ser::Action action) {
    outcome::result<void> result{outcome::success()};
    if (action == ser::Action::kSerialize) {
        uint32_t type{static_cast<uint32_t>(type_)};
        result = stream.bind(type, action);
        if (not result.has_error()) result = stream.bind(identifier_, action);
    } else {
        uint32_t type{0};
        result = stream.bind(type, action);
        if (not result.has_error()) {
            auto enumerator{magic_enum::enum_cast<Type>(type)};
            if (not enumerator.has_value()) return outcome::failure(ser::Error::kInvalidInventoryType);
            type_ = enumerator.value();
            result = stream.bind(identifier_, action);
        }
    }
    return result;
}
}  // namespace znode