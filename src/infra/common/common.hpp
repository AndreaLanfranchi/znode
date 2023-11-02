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

#pragma once
#include <exception>

#include <core/common/outcome.hpp>

namespace znode {

//! \brief Throws exception if result has error or exception
template <typename T>
inline void success_or_throw(const outcome::result<T>& result) {
    if (result.has_failure()) [[unlikely]] {
        throw boost::system::system_error(result.error());
    }
}

//! \brief Throws exception if error is not success
inline void success_or_throw(const boost::system::error_code& error) {
    if (error) [[unlikely]] {
        throw boost::system::system_error(error);
    }
}
}  // namespace znode
