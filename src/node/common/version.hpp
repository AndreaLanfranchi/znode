/*
   Copyright 2022 The Silkworm Authors
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
#include <cstdint>

#include <absl/strings/str_cat.h>
namespace znode {

//! \brief Used to compare versions of entities (e.g. Db schema version)
struct Version {
    uint32_t Major{0};
    uint32_t Minor{0};
    uint32_t Patch{0};
    [[nodiscard]] std::string to_string() const { return absl::StrCat(Major, ".", Minor, ".", Patch); }
    friend auto operator<=>(const Version&, const Version&) = default;
};

}  // namespace znode
