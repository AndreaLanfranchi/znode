/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <limits>
#include <optional>
#include <type_traits>

namespace zenpp {

template <typename T>
constexpr bool is_overflow_safe(T a, T b) noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return a <= std::numeric_limits<T>::max() - b;
    } else if constexpr (std::is_signed_v<T>) {
        if constexpr (std::is_signed_v<decltype(a + b)>) {
            return (b >= 0 && a <= std::numeric_limits<T>::max() - b) ||
                   (b < 0 && a >= std::numeric_limits<T>::min() - b);
        } else {
            return a <= std::numeric_limits<T>::max() - b;
        }
    } else {
        return a <= std::numeric_limits<T>::max() - b;
    }
}

template <typename T>
[[nodiscard]] std::optional<T> safe_add(T a, T b) noexcept {
    if (is_overflow_safe(a, b)) [[likely]] {
        return a + b;
    }
    return std::nullopt;
}

template <typename T>
[[nodiscard]] T saturating_add(T a, T b) noexcept {
    if (is_overflow_safe(a, b)) [[likely]] {
        return a + b;
    }
    return std::numeric_limits<T>::max();
}
}  // namespace zenpp
