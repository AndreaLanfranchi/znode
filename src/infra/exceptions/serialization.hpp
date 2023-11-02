/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <exception>
#include <stdexcept>
#include <string>

#include <magic_enum.hpp>

#include <core/serialization/base.hpp>

namespace znode::ser {

class Exception : public std::logic_error {
  public:
    using std::logic_error::logic_error;
    static void success_or_throw(Error err) {
        if (err == Error::kSuccess) return;
        throw Exception(std::string(magic_enum::enum_name(err)));
    }
};

}  // namespace znode::ser
