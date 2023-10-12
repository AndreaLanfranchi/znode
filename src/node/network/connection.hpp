/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <infra/network/addresses.hpp>

namespace zenpp::net {

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

}  // namespace zenpp::net