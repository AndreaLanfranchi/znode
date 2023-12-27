/*
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

#include "protocol.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>
#include <core/common/cast.hpp>

namespace znode::net {
namespace {

    Bytes get_command_from_message_type(MessageType message_type, bool check_length = true) noexcept {
        std::string label(magic_enum::enum_name(message_type));
        label.erase(0, 1);  // get rid of the 'k' prefix
        if (check_length) {
            ASSERT(label.size() <= kMessageHeaderCommandLength and "Message command label too long");
        }
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return std::tolower(c); });
        Bytes ret{label.begin(), label.end()};
        ret.resize(kMessageHeaderCommandLength, 0);
        return ret;
    }

    std::vector<std::pair<Bytes, MessageType>> get_commands() {
        std::vector<std::pair<Bytes, MessageType>> ret;
        for (const auto enumerator : magic_enum::enum_values<MessageType>()) {
            if (enumerator == MessageType::kMissingOrUnknown) continue;
            ret.emplace_back(get_command_from_message_type(enumerator), enumerator);
        }
        return ret;
    }

}  // namespace

MessageType message_type_from_command(const std::array<uint8_t, kMessageHeaderCommandLength>& command) {
    static thread_local const auto commands{get_commands()};
    for (const auto& [command_label, message_type] : commands) {
        if (memcmp(command_label.data(), command.data(), command.size()) == 0) {
            return message_type;
        }
    }
    return MessageType::kMissingOrUnknown;
}

bool is_known_command(const std::string& command) noexcept {
    if (command.empty() or command.size() > kMessageHeaderCommandLength) return false;
    std::array<uint8_t, kMessageHeaderCommandLength> command_bytes{0};
    std::copy(command.begin(), command.end(), command_bytes.begin());
    return message_type_from_command(command_bytes) not_eq MessageType::kMissingOrUnknown;
}

std::string command_from_message_type(MessageType message_type, bool check_length) noexcept {
    auto command{get_command_from_message_type(message_type, check_length)};
    auto ret{std::string{byte_view_to_string_view(command)}};
    ret.erase(std::find_if(ret.rbegin(), ret.rend(), [](unsigned char c) { return c != 0; }).base(), ret.end());
    return ret;
}
}  // namespace znode::net
