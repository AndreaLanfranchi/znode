/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <zen/core/common/base.hpp>

namespace zen::crypto {

class Jenkins {
  public:
    static uint64_t Hash(const uint32_t* source, size_t length, const uint32_t* salt);

  private:
    static void HashMix(uint32_t& a, uint32_t& b, uint32_t& c);
    static void HashFinal(uint32_t& a, uint32_t& b, uint32_t& c);
};

}  // namespace zen::crypto
