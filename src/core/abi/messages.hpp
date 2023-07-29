/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <core/abi/netmessage.hpp>
#include <core/types/address.hpp>

namespace zenpp::abi {

class NullData : public serialization::Serializable {
  public:
    NullData() = default;
    ~NullData() override = default;

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream&, serialization::Action) override {
        // Nothing to (de)serialize here
        return serialization::Error::kSuccess;
    };
};

class Version : public serialization::Serializable {
  public:
    Version() = default;
    ~Version() override = default;

    int32_t version{0};
    uint64_t services{static_cast<uint64_t>(NetworkServicesType::kNone)};
    int64_t timestamp{0};
    NetworkAddress addr_recv{};
    NetworkAddress addr_from{};
    uint64_t nonce{0};
    std::string user_agent{};
    int32_t start_height{0};
    bool relay{false};

  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};
}  // namespace zenpp::abi