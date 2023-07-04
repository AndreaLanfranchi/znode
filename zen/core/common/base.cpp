/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>

#include <zen/core/common/base.hpp>

#if defined(BOOST_NO_EXCEPTIONS)
namespace boost {
void throw_exception(const std::exception&) {
    std::cerr << "Aborted due to unallowed exception" << std::endl;
    std::abort();
}
}  // namespace boost
#endif

namespace zen {

//! \brief Portable strnlen_s
#if !defined(_MSC_VER)
unsigned long long strnlen_s(const char* str, size_t strsz) noexcept {
    if (str == nullptr) return 0;
    const char* end = static_cast<const char*>(std::memchr(str, 0, strsz));
    return end ? static_cast<unsigned long long>(end - str) : strsz;
}
#endif

}  // namespace zen
