/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <core/serialization/serializable.hpp>

namespace zenpp::abi {

class MessageVersion : public serialization::Serializable {
  public:
    int32_t version;
    uint64_t services;
    int64_t timestamp;
  private:
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

}  // namespace zenpp::abi