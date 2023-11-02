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
#include <optional>

#include <core/common/base.hpp>

namespace znode::env {

//! \brief Get environment variable value
std::optional<std::string> get(const std::string& name) noexcept;

//! \brief Gets the default storage path for the OS
std::optional<std::string> get_default_storage_path();

//! \brief Set environment variable value
void set(const std::string& name, const std::string& value);

}  // namespace znode::env
