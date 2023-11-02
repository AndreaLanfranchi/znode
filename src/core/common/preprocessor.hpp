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

// Only 64 bit little endian arch allowed
#if defined(_MSC_VER) || (defined(__INTEL_COMPILER) && defined(_WIN32))
#if defined(_M_X64)
#define BITNESS_64
#else
#define BITNESS_32
#endif
#elif defined(__clang__) || defined(__INTEL_COMPILER) || defined(__GNUC__)
#if defined(__x86_64)
#define BITNESS_64
#else
#define BITNESS_32
#endif
#else
#error Cannot detect compiler or compiler is not supported
#endif
#if !defined(BITNESS_64)
#error "Only 64 bit target architecture is supported"
#endif
#undef BITNESS_32
#undef BITNESS_64

#if defined(__wasm__)
#define ZEN_THREAD_LOCAL
#else
#define ZEN_THREAD_LOCAL thread_local
#endif

// Inlining
#if defined(__GNUC__) || defined(__clang__)
#define ZEN_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
#define ZEN_ALWAYS_INLINE __forceinline
#define __func__ __FUNCTION__
#else
#define ZEN_ALWAYS_INLINE
#endif