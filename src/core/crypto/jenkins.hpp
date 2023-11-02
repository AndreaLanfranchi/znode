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

class Jenkins {
  public:
    static uint64_t Hash(const uint32_t* source, size_t length, const uint32_t* salt);

  private:
    static void HashMix(uint32_t& a, uint32_t& b, uint32_t& c);
    static void HashFinal(uint32_t& a, uint32_t& b, uint32_t& c);
};

}  // namespace znode::crypto
