/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <string_view>

#include <zen/core/common/preprocessor.hpp>

namespace zen {
[[noreturn]] void abort_due_to_assertion_failure(std::string_view message, const char* function, const char* file,
                                                 long line);
}  // namespace zen

//! \brief Always aborts program execution on assertion failure, even when NDEBUG is defined.
#define ZEN_ASSERT(expr)      \
    if ((expr)) [[likely]]    \
        static_cast<void>(0); \
    else                      \
        ::zen::abort_due_to_assertion_failure(#expr, __func__, __FILE__, __LINE__)

//! \brief An alias for ZEN_ASSERT to make semantically clear that we're checking a precondition.
#define ZEN_REQUIRE(expr) ZEN_ASSERT(expr)

//! \brief An alias for ZEN_ASSERT to make semantically clear that we're checking a postcondition.
#define ZEN_ENSURE(expr) ZEN_ASSERT(expr)