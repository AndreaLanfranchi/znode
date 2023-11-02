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

#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <infra/network/addresses.hpp>

namespace znode::net {

enum class ConnectionType : uint8_t {
    kNone = 0U,            // Unspecified
    kInbound = 1U,         // Dial-in
    kOutbound = 2U,        // Dial-out
    kManualOutbound = 3U,  // Dial-out initiated by user via CLI or RPC call
    kSeedOutbound = 4U,    // Dial-out initiated by process to query seed nodes
};

class Connection {
  public:
    Connection() = default;
    ~Connection() = default;

    Connection(const IPEndpoint& endpoint, ConnectionType type) noexcept : endpoint_{endpoint}, type_{type} {
        ASSERT(type_ not_eq ConnectionType::kNone);
    };

    Connection(const boost::asio::ip::tcp::endpoint& endpoint, ConnectionType type) noexcept
        : endpoint_{endpoint}, type_{type} {
        ASSERT(type_ not_eq ConnectionType::kNone);
    };

    Connection(const boost::asio::ip::address& address, uint16_t port_num, ConnectionType type) noexcept
        : endpoint_{address, port_num}, type_{type} {
        ASSERT(type_ not_eq ConnectionType::kNone);
    };

    Connection(boost::asio::ip::address address, uint16_t port_num, ConnectionType type) noexcept
        : endpoint_{std::move(address), port_num}, type_{type} {
        ASSERT(type_ not_eq ConnectionType::kNone);
    };

    Connection(const Connection& other) = default;

    Connection& operator=(const Connection& other) {
        if (this != &other) {
            endpoint_ = other.endpoint_;
            type_ = other.type_;
        }
        return *this;
    }

    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            endpoint_ = other.endpoint_;
            type_ = other.type_;
        }
        return *this;
    }

    bool operator==(const Connection& other) const noexcept = default;

    IPEndpoint endpoint_{};
    ConnectionType type_{ConnectionType::kNone};
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr_{nullptr};
};

}  // namespace znode::net