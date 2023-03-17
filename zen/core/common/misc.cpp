/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <array>
#include <random>
#include <regex>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include <zen/core/common/misc.hpp>

namespace zen {

std::string abridge(std::string_view input, size_t length) {
    if (!length || input.empty()) return std::string(input);
    if (input.length() <= length) {
        return std::string(input);
    }
    return std::string(input.substr(0, length)) + "...";
}

tl::expected<uint64_t, DecodingError> parse_human_bytes(const std::string& input) {
    if (input.empty()) {
        return 0ULL;
    }

    static const std::regex pattern{R"(^(\d{0,10})(\.\d{1,3})?\ *?(B|KB|MB|GB|TB|KiB|MiB|GiB|TiB)?$)",
                                    std::regex_constants::icase};
    std::smatch matches;
    if (!std::regex_search(input, matches, pattern, std::regex_constants::match_default)) {
        return tl::unexpected{DecodingError::kInvalidInput};
    }

    uint64_t multiplier{1};  // Default for bytes (B|b)

    std::string whole_part{matches[1].str()};
    std::string fract_part{matches[2].str()};
    std::string suffix{matches[3].str()};
    if (!fract_part.empty()) {
        fract_part.erase(fract_part.begin());
    }

    if (boost::iequals(suffix, "KB")) {
        multiplier = kKB;
    } else if (boost::iequals(suffix, "MB")) {
        multiplier = kMB;
    } else if (boost::iequals(suffix, "GB")) {
        multiplier = kGB;
    } else if (boost::iequals(suffix, "TB")) {
        multiplier = kTB;
    } else if (boost::iequals(suffix, "KiB")) {
        multiplier = kKiB;
    } else if (boost::iequals(suffix, "MiB")) {
        multiplier = kMiB;
    } else if (boost::iequals(suffix, "GiB")) {
        multiplier = kGiB;
    } else if (boost::iequals(suffix, "TiB")) {
        multiplier = kTiB;
    }

    auto value{std::strtoull(whole_part.c_str(), nullptr, 10)};
    value *= multiplier;
    if (multiplier > 1 && !fract_part.empty()) {
        uint32_t base{10};
        for (size_t i{1}; i < fract_part.size() /* up to this decimal places */; ++i) {
            base *= 10;
        }
        const auto fract{std::strtoul(fract_part.c_str(), nullptr, 10)};
        value += multiplier * fract / base;
    }
    return value;
}

std::string to_human_bytes(const size_t input, bool binary) {
    static const std::array<const char*, 5> suffixes{"B", "KB", "MB", "GB", "TB"};             // Must have same ..
    static const std::array<const char*, 5> binary_suffixes{"B", "KiB", "MiB", "GiB", "TiB"};  // ...number of items
    const auto divisor{binary ? kKiB : kKB};
    uint32_t index{0};
    double value{static_cast<double>(input)};
    while (value >= divisor && index < suffixes.size()) {
        value /= divisor;
        ++index;
    }

    // TODO(C++20/23) Replace with std::format when widely available on GCC and Clang
    std::string formatter{index ? "%.02f %s" : "%.0f %s"};
    return boost::str(boost::format(formatter) % value % (binary ? binary_suffixes[index] : suffixes[index]));
}

std::string get_random_alpha_string(size_t length) {
    static constexpr std::string_view kAlphaNum{
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"};

    std::random_device rd;
    std::default_random_engine engine{rd()};
    std::uniform_int_distribution<size_t> uniform_dist{0, kAlphaNum.size() - 1};

    std::string ret(length, '0');
    for (size_t i{0}; i < length; ++i) {
        const size_t random_number{uniform_dist(engine)};
        ret[i] = kAlphaNum[random_number];
    }

    return ret;
}
}  // namespace zen
