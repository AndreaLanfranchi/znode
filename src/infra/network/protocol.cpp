/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "protocol.hpp"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include <core/common/assert.hpp>
#include <core/common/base.hpp>

namespace zenpp::net {
namespace {

    Bytes get_command_from_message_type(MessageType message_type) noexcept {
        std::string label(magic_enum::enum_name(message_type));
        label.erase(0, 1);  // get rid of the 'k' prefix
        ASSERT(not label.empty() and label.size() <= kMessageHeaderCommandLength and "Message command label too long");
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
}  // namespace zenpp::net
