/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <limits>

namespace zenpp::net {

static constexpr int32_t kDefaultProtocolVersion{170'002};       // Our default protocol version
static constexpr int32_t kMinSupportedProtocolVersion{170'002};  // Min acceptable protocol version
static constexpr int32_t kMaxSupportedProtocolVersion{
    std::numeric_limits<int32_t>::max()};  // Max acceptable protocol version

}  // namespace zenpp::net
