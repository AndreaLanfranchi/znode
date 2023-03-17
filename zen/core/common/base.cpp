/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>

#include <zen/core/common/base.hpp>

#if defined(BOOST_NO_EXCEPTIONS)
void boost::throw_exception(const std::exception&) {
    std::cerr << "Aborted due to unallowed exception" << std::endl;
    std::abort();
};
#endif
