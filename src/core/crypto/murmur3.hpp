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

#include <core/common/base.hpp>

namespace znode::crypto {

class Murmur3 {
  public:
    //! \brief Murmur3 32-bit hash function
    //! \see https://en.wikipedia.org/wiki/MurmurHash
    //! \see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
    static uint32_t Hash(const uint32_t seed, ByteView data);
};

}  // namespace znode::crypto
