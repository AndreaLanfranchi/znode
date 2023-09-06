/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <ranges>

#include <absl/strings/str_cat.h>
#include <boost/algorithm/string/predicate.hpp>
#include <magic_enum.hpp>

#include <core/chain/config.hpp>

namespace zenpp {

namespace {
    const std::vector<std::pair<std::string, const ChainConfig*>> kKnownChainConfigs{
        {"mainnet", &kMainNetConfig},
        {"testnet", &kTestNetConfig},
        {"regtest", &kRegTestConfig},
    };
}

nlohmann::json ChainConfig::to_json() const noexcept {
    nlohmann::json ret;
    ret["chainId"] = identifier_;
    ret["chainName"] = kKnownChainConfigs[identifier_ - 1].first;  // TODO: this is a hack, fix it [1]
    ret["chainMagic"] = magic_;
    ret["defaultPort"] = default_port_;

    nlohmann::json consensus_object(nlohmann::json::value_t::object);
    auto consensus_name{std::string(magic_enum::enum_name(seal_engine_type_))};
    consensus_name.erase(consensus_name.find('k'), 1);
    consensus_object[consensus_name] = nlohmann::json::object();
    consensus_object[consensus_name]["K"] = 200U;  // TODO: this is not constant
    consensus_object[consensus_name]["N"] = 9U;    // TODO: this is not constant

    ret["consensus"] = consensus_object;
    return ret;
}

std::optional<ChainConfig> ChainConfig::from_json(const nlohmann::json& json) noexcept {
    if (json.is_discarded() or not json.contains("chainId") or not json["chainId"].is_number_integer()) {
        return std::nullopt;
    }

    ChainConfig config{};
    config.identifier_ = json["chainId"].get<uint32_t>();
    config.magic_ = json["chainMagic"].get<std::array<uint8_t, 4>>();
    config.default_port_ = json["defaultPort"].get<uint16_t>();

    if (json.contains("consensus")) {
        auto consensus_object{json["consensus"].get<nlohmann::json>()};
        if (consensus_object.is_object()) {
            for (const auto& [key, value] : consensus_object.items()) {
                if (value.is_object()) {
                    const auto consensus_name{absl::StrCat("k", key)};
                    config.seal_engine_type_ = magic_enum::enum_cast<SealEngineType>(consensus_name).value();
                }
            }
        }
    }

    return config;
}

std::ostream& operator<<(std::ostream& out, const ChainConfig& obj) { return out << obj.to_json(); }

std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(const uint32_t identifier) noexcept {
    auto iterator{std::ranges::find_if(kKnownChainConfigs,
                                       [&identifier](const std::pair<std::string, const ChainConfig*>& pair) -> bool {
                                           return pair.second->identifier_ == identifier;
                                       })};

    if (iterator == kKnownChainConfigs.end()) {
        return std::nullopt;
    }
    return std::pair(*iterator);
}

std::optional<std::pair<const std::string, const ChainConfig*>> lookup_known_chain(
    const std::string_view identifier) noexcept {
    auto iterator{std::ranges::find_if(kKnownChainConfigs,
                                       [&identifier](const std::pair<std::string, const ChainConfig*>& pair) -> bool {
                                           return boost::iequals(pair.first, identifier);
                                       })};

    if (iterator == kKnownChainConfigs.end()) {
        return std::nullopt;
    }
    return std::pair(*iterator);
}

std::string lookup_known_chain_name(uint32_t identifier) noexcept {
    const auto chain{lookup_known_chain(identifier)};
    if (not chain.has_value()) return "unknown";
    return chain.value().first;
}

std::map<std::string, uint32_t> get_known_chains_map() noexcept {
    std::map<std::string, uint32_t> ret;
    std::ranges::for_each(kKnownChainConfigs, [&ret](const std::pair<std::string, const ChainConfig*>& pair) -> void {
        ret[pair.first] = pair.second->identifier_;
    });
    return ret;
}

}  // namespace zenpp
