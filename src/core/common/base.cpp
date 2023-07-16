/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>

#include <core/common/base.hpp>

#if defined(BOOST_NO_EXCEPTIONS)
namespace boost {
void throw_exception(const std::exception&) {
    std::cerr << "Aborted due to unallowed exception" << std::endl;
    std::abort();
}
}  // namespace boost
#endif

namespace zenpp {

//! \brief Portable strnlen_s
#if !defined(_MSC_VER)
unsigned long long strnlen_s(const char* str, size_t strsz) noexcept {
    if (str == nullptr) return 0;
    const char* end = static_cast<const char*>(std::memchr(str, 0, strsz));
    return end ? static_cast<unsigned long long>(end - str) : strsz;
}
#endif

const buildinfo* get_buildinfo() noexcept { return zenpp_get_buildinfo(); }

std::string get_buildinfo_string() noexcept {
    std::string ret{};
    const auto* build_info = get_buildinfo();
    ret.append(build_info->project_name_with_version);
    ret.append(" ");
    ret.append(build_info->system_name);
    ret.append("-");
    ret.append(build_info->system_processor);
    ret.append("_");
    ret.append(build_info->build_type);
    ret.append("/");
    ret.append(build_info->compiler_id);
    ret.append("-");
    ret.append(build_info->compiler_version);
    return ret;
}
}  // namespace zenpp
