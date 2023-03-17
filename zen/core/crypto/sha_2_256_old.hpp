/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <cstdint>

namespace zen::crypto {
//! \brief A wrapper around OpenSSL's SHA256 crypto functions
class Sha256Old {
  public:
    static const size_t OUTPUT_SIZE = 32;

    Sha256Old();
    Sha256Old& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    void FinalizeNoPadding(unsigned char hash[OUTPUT_SIZE]) { FinalizeNoPadding(hash, true); };
    Sha256Old& Reset();

  private:
    uint32_t s[8];
    unsigned char buf[64];
    size_t bytes;
    void FinalizeNoPadding(unsigned char hash[OUTPUT_SIZE], bool enforce_compression);
};
}  // namespace zen::crypto
