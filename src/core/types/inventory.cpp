/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
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