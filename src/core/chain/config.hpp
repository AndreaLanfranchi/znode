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
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace znode {

enum class SealEngineType {
    kNoProof,
    kEquihash
};

class ChainConfig {
  public:
    uint32_t identifier_{0};           // A numeric identifier for the chain (lately mapped to a string)
    std::array<uint8_t, 4> magic_{0};  // The magic bytes to identify the chain on messages
    uint16_t default_port_{0};         // The default port to use for peer-to-peer communication
    SealEngineType seal_engine_type_{SealEngineType::kNoProof};  // The type of seal engine used by the chain

    //! \brief Returns the JSON representation of the chain configuration
    [[nodiscard]] nlohmann::json to_json() const noexcept;

    //! \brief Try parse a JSON object into strongly typed ChainConfig
    //! \remark Should this return std::nullopt the parsing has failed
    [[nodiscard]] static std::optional<ChainConfig> from_json(const nlohmann::json& json) noexcept;
};

std::ostream& operator<<(std::ostream& out, const ChainConfig& obj);

inline constexpr ChainConfig kMainNetConfig{.identifier_ = 1U,
                                            .magic_ = {0x63U, 0x61U, 0x73U, 0x68U},
                                            .default_port_ = 9033U,
                                            .seal_engine_type_ = SealEngineType::kEquihash};

inline constexpr ChainConfig kTestNetConfig{.identifier_ = 2U,
                                            .magic_ = {0xbfU, 0xf2U, 0xcdU, 0xe6U},
                                            .default_port_ = 19033U,
                                            .seal_engine_type_ = SealEngineType::kEquihash};

inline constexpr ChainConfig kRegTestConfig{.identifier_ = 3U,
                                            .magic_ = {0x2fU, 0x54U, 0xccU, 0x9dU},
                                            .default_port_ = 19133U,
                                            .seal_engine_type_ = SealEngineType::kEquihash};

//! \brief Looks up a known chain config provided its chain ID
std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(uint32_t identifier) noexcept;

//! \brief Looks up a known chain name provided its chain ID
//! \remark Should the provided chain ID not be known, the constant "unknown" is returned
std::string lookup_known_chain_name(uint32_t identifier) noexcept;

//! \brief Looks up a known chain config provided its chain identifier (eg. "mainnet")
std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(
    std::string_view identifier) noexcept;

//! \brief Returns a map known chains names mapped to their respective chain ids
std::map<std::string, uint32_t> get_known_chains_map() noexcept;

}  // namespace znode
