/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
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

namespace zenpp {

enum class SealEngineType {
    kNoProof,
    kEquihash
};

class ChainConfig {
  public:
    uint32_t identifier_{0};           // A numeric identifier for the chain (lately mapped to a string)
    std::array<uint8_t, 4> magic_{0};  // The magic bytes to identify the chain on messages
    uint32_t default_port_{0};         // The default port to use for peer-to-peer communication
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
std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(uint32_t pair) noexcept;

//! \brief Looks up a known chain config provided its chain identifier (eg. "mainnet")
std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(std::string_view pair) noexcept;

//! \brief Returns a map known chains names mapped to their respective chain ids
std::map<std::string, uint32_t> get_known_chains_map() noexcept;

}  // namespace zenpp
