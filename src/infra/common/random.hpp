/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include "core/common/base.hpp"

#include <limits>
#include <random>

namespace znode {

//! \brief Generates a random value of type T in a provided [min..max] range
template <Integral T>
T randomize(const T min, const T max) {
    ZEN_THREAD_LOCAL std::random_device rnd;
    ZEN_THREAD_LOCAL std::mt19937 gen(rnd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

//! \brief Generates a random value of type T in range [min..std::numeric_limits<T>::max()]
template <Integral T>
T randomize(const T min) {
    return randomize<T>(static_cast<T>(min), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range [std::numeric_limits<T>::max()..std::numeric_limits<T>::max()]
template <Integral T>
T randomize() {
    return randomize<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range (T * (1.0F - percentage), T * (1.0F + percentage))
//! \remarks If the value is 0 or percentage is 0.0, the value is returned as is
//! \remarks Function is over/underflow safe
template <Integral T>
T randomize(T val, double percentage) {
    if (val == T(0) or percentage == 0.0) return val;
    percentage = std::max<double>(1.0, std::abs(percentage));
    T abs_value{val};
    if constexpr (std::is_signed_v<T>) {
        abs_value = static_cast<T>(val < 0 ? -val : val);
    }
    const znode::Integral auto variance = static_cast<T>(abs_value * percentage);
    if (variance == T(0)) return val;
    const T min =
        (val > (std::numeric_limits<T>::lowest() + variance)) ? val - variance : std::numeric_limits<T>::lowest();
    const T max = ((std::numeric_limits<T>::max() - variance) > val) ? val + variance : std::numeric_limits<T>::max();
    return randomize<T>(min, max);
}

Bytes get_random_bytes(size_t size);

}  // namespace znode