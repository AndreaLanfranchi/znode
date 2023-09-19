/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <optional>

#include <core/common/base.hpp>

namespace zenpp::env {

//! \brief Get environment variable value
std::optional<std::string> get(const std::string& name) noexcept;

//! \brief Gets the default storage path for the OS
std::optional<std::string> get_default_storage_path();

//! \brief Set environment variable value
void set(const std::string& name, const std::string& value);

}  // namespace zenpp::env
