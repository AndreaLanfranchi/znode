/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <limits>
#include <random>

#include <core/common/base.hpp>

namespace zenpp {

//! \brief Generates a random value of type T in a provided (min, max) range
template <Integral T>
T randomize(const T min, const T max) {
    ZEN_THREAD_LOCAL std::random_device rnd;
    ZEN_THREAD_LOCAL std::mt19937 gen(rnd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

//! \brief Generates a random value of type T in range (min,std::numeric_limits<T>::max())
template <Integral T>
T randomize(const T min) {
    return randomize<T>(static_cast<T>(min), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range (std::numeric_limits<T>::max(),std::numeric_limits<T>::max())
template <Integral T>
T randomize() {
    return randomize<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

//! \brief Generates a random value of type T in range (T * (1.0F - percentage), T * (1.0F + percentage))
template <Integral T>
T randomize(T val, float percentage) {
    percentage = std::abs(percentage);
    if (percentage > 1.0F) percentage = 1.0F;
    const T min = static_cast<T>(val * (1.0F - percentage));
    const T max = static_cast<T>(val * (1.0F + percentage));
    return randomize<T>(min, max);
}

}  // namespace zenpp