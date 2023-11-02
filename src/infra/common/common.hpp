/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
