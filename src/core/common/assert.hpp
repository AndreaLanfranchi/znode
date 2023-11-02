/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <string_view>
#if __has_include(<source_location>)
#include <source_location>
#elif __has_include(<experimental/source_location>)
#include <experimental/source_location>
namespace std {
using source_location = std::experimental::source_location;
}
#else
#error "Missing <source_location>"
#endif

#include <core/common/preprocessor.hpp>

namespace znode {
[[noreturn]] void abort_due_to_assertion_failure(std::string_view message, const std::source_location& location);
}  // namespace znode

//! \brief Always aborts program execution on assertion failure, even when NDEBUG is defined.
#define ASSERT(expr)          \
    if ((expr)) [[likely]]    \
        static_cast<void>(0); \
    else                      \
        abort_due_to_assertion_failure(#expr, std::source_location::current())

//! \brief An alias for ASSERT with semantic emphasis on pre-condition validation.
#define ASSERT_PRE(expr) ASSERT(expr)

//! \brief An alias for ASSERT with semantic emphasis on post-condition validation.
#define ASSERT_POST(expr) ASSERT(expr)