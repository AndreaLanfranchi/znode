/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <limits>

namespace zenpp::net {

inline constexpr int kDefaultProtocolVersion{170'002};                               // Our protocol version
inline constexpr int kMinSupportedProtocolVersion{170'002};                          // Min acceptable protocol version
inline constexpr int kMaxSupportedProtocolVersion{std::numeric_limits<int>::max()};  // Max acceptable protocol version

}  // namespace zenpp::net
