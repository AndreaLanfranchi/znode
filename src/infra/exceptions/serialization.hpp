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

#include <exception>
#include <stdexcept>
#include <string>

#include <magic_enum.hpp>

#include <core/serialization/base.hpp>

namespace znode::ser {

class Exception : public std::logic_error {
  public:
    using std::logic_error::logic_error;
    static void success_or_throw(Error err) {
        if (err == Error::kSuccess) return;
        throw Exception(std::string(magic_enum::enum_name(err)));
    }
};

}  // namespace znode::ser
