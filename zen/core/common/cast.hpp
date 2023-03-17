/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstring>

#include <zen/core/common/base.hpp>

// Utilities for type casting

namespace zen {

// Cast between pointers to char and unsigned char (i.e. uint8_t)

inline char* byte_ptr_cast(uint8_t* ptr) { return reinterpret_cast<char*>(ptr); }

inline const char* byte_ptr_cast(const uint8_t* ptr) { return reinterpret_cast<const char*>(ptr); }

inline uint8_t* byte_ptr_cast(char* ptr) { return reinterpret_cast<uint8_t*>(ptr); }

inline const uint8_t* byte_ptr_cast(const char* ptr) { return reinterpret_cast<const uint8_t*>(ptr); }

inline ByteView string_view_to_byte_view(std::string_view v) { return {byte_ptr_cast(v.data()), v.length()}; }

inline std::string_view byte_view_to_string_view(ByteView v) { return {byte_ptr_cast(v.data()), v.length()}; }

}  // namespace zen
