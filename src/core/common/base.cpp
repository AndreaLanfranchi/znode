/*
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

#include "base.hpp"

#include <iostream>

#if defined(BOOST_NO_EXCEPTIONS)
namespace boost {
void throw_exception(const std::exception& /*unused*/) {
    std::cerr << "Aborted due to unallowed exception" << std::endl;
    std::abort();
}
void throw_exception(const std::exception& /*unused*/, const boost::source_location& /*unused*/) {
    std::cerr << "Aborted due to unallowed exception" << std::endl;
    std::abort();
}
}  // namespace boost
#endif

namespace znode {

//! \brief Portable strnlen_s
#if !defined(_MSC_VER)
unsigned long long strnlen_s(const char* str, size_t strsz) noexcept {
    if (str == nullptr) return 0;
    const char* end = static_cast<const char*>(std::memchr(str, 0, strsz));
    return end ? static_cast<unsigned long long>(end - str) : strsz;
}
#endif

const buildinfo* get_buildinfo() noexcept { return znode_get_buildinfo(); }

std::string get_buildinfo_string() noexcept {
    std::string ret{};
    const auto* build_info = get_buildinfo();
    ret.append(build_info->project_name);
    ret.append(" ");
    ret.append(build_info->project_version);
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
}  // namespace znode
