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
#include <core/encoding/errors.hpp>

namespace znode::enc::base64 {

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] outcome::result<std::string> encode(ByteView bytes) noexcept;

//! \brief Returns a string of ascii chars with the base64 representation of input
//! \remark If provided an empty input the return string is empty as well
[[nodiscard]] outcome::result<std::string> encode(std::string_view data) noexcept;

//! \brief Returns a string of bytes with the decoded base64 payload
//! \remark If provided an empty input the returned bytes are empty as well
[[nodiscard]] outcome::result<Bytes> decode(std::string_view input) noexcept;

}  // namespace znode::enc::base64
