/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>

#include <absl/strings/str_cat.h>
namespace zen {

//! \brief Used to compare versions of entities (e.g. Db schema version)
struct Version {
    uint32_t Major{0};
    uint32_t Minor{0};
    uint32_t Patch{0};
    [[nodiscard]] std::string to_string() const { return absl::StrCat(Major, ".", Minor, ".", Patch); }
    friend auto operator<=>(const Version&, const Version&) = default;
};

}  // namespace zen
