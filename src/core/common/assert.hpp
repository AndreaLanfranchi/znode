/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <string_view>

#include <core/common/preprocessor.hpp>

namespace zenpp {
[[noreturn]] void abort_due_to_assertion_failure(std::string_view message, const char* function, const char* file,
                                                 long line);
}  // namespace zenpp

//! \brief Always aborts program execution on assertion failure, even when NDEBUG is defined.
#define ASSERT(expr)          \
    if ((expr)) [[likely]]    \
        static_cast<void>(0); \
    else                      \
        ::zenpp::abort_due_to_assertion_failure(#expr, __func__, __FILE__, __LINE__)

//! \brief An alias for ASSERT to make semantically clear that we're checking a precondition.
#define REQUIRES(expr) ASSERT(expr)

//! \brief An alias for ASSERT to make semantically clear that we're checking a postcondition.
#define ENSURE(expr) ASSERT(expr)