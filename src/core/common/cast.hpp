/*
   Copyright 2022 The Silkworm Authors
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
#include <cstring>

#include <core/common/base.hpp>

// Utilities for type casting

namespace znode {

// Cast between pointers to char and unsigned char (i.e. uint8_t)

inline char* byte_ptr_cast(uint8_t* ptr) { return reinterpret_cast<char*>(ptr); }

inline const char* byte_ptr_cast(const uint8_t* ptr) { return reinterpret_cast<const char*>(ptr); }

inline uint8_t* byte_ptr_cast(char* ptr) { return reinterpret_cast<uint8_t*>(ptr); }

inline const uint8_t* byte_ptr_cast(const char* ptr) { return reinterpret_cast<const uint8_t*>(ptr); }

inline ByteView string_view_to_byte_view(std::string_view view) { return {byte_ptr_cast(view.data()), view.length()}; }

inline std::string_view byte_view_to_string_view(ByteView view) { return {byte_ptr_cast(view.data()), view.length()}; }

}  // namespace znode
