/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "misc.hpp"

#include <array>
#include <regex>
#include <set>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <gsl/gsl_util>

#include <core/common/random.hpp>

namespace zenpp {

std::string abridge(std::string_view input, size_t length) {
    if (length == 0U or input.empty()) return std::string(input);
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

    const std::string whole_part{matches[1].str()};
    std::string fract_part{matches[2].str()};
    const std::string suffix{matches[3].str()};
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
    const auto divisor{gsl::narrow_cast<float>(binary ? kKiB : kKB)};
    uint32_t index{0};
    double value{static_cast<double>(input)};
    while (value >= divisor and index < gsl::narrow_cast<uint32_t>(suffixes.size())) {
        value /= divisor;
        ++index;
    }

    // TODO(C++20/23) Replace with std::format when widely available on GCC and Clang
    const std::string formatter{index > 0U ? "%.02f %s" : "%.0f %s"};
    return boost::str(boost::format(formatter) % value % (binary ? binary_suffixes[index] : suffixes[index]));
}

std::string get_random_alpha_string(size_t length) {
    static constexpr std::string_view kAlphaNum{"0123456789abcdefghijklmnopqrstuvwxyz"};

    std::string ret(length, '0');
    for (size_t i{0}; i < length; ++i) {
        const size_t random_number{randomize<size_t>(0, kAlphaNum.size() - 1)};
        ret[i] = kAlphaNum[random_number];
    }

    return ret;
}

size_t count_duplicate_data_chunks(ByteView data, const size_t chunk_size, const size_t max_count) noexcept {
    if (chunk_size == 0U or data.length() < chunk_size) {
        return 0;
    }
    std::set<ByteView, std::less<>> unique_chunks;
    size_t count{0};
    const size_t chunks{data.length() / chunk_size};
    for (size_t i{0}; i < chunks; ++i) {
        const auto chunk{data.substr(i * chunk_size, chunk_size)};
        if (not unique_chunks.insert(chunk).second) {
            ++count;
            if (max_count not_eq 0U and count == max_count) {
                break;
            }
        }
    }
    return count;
}

bool try_parse_ip_address_and_port(std::string_view input, boost::asio::ip::address& address, uint16_t& port) noexcept {
    if (input.empty()) return false;

    static const std::regex ipv4_pattern(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})(?::(\d+))?)");
    static const std::regex ipv6_pattern(R"(\[?([0-9a-f:]+)\]?(?::(\d+))?)", std::regex_constants::icase);
    static const std::regex ipv6_ipv4_pattern(R"(\[?::ffff:((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))\]?(?::(\d+))?)",
                                              std::regex_constants::icase);

    std::smatch matches;
    boost::system::error_code error_code;
    const std::string input_str{input};
    if (std::regex_match(input_str, matches, ipv4_pattern)) {
        address = boost::asio::ip::make_address_v4(matches[1].str(), error_code);
        port = matches[2].matched ? gsl::narrow_cast<uint16_t>(std::stoul(matches[2].str())) : port;
        return !error_code;
    }
    if (std::regex_match(input_str, matches, ipv6_pattern)) {
        address = boost::asio::ip::make_address_v6(matches[1].str(), error_code);
        port = matches[2].matched ? gsl::narrow_cast<uint16_t>(std::stoul(matches[2].str())) : port;
        return !error_code;
    }
    if (std::regex_match(input_str, matches, ipv6_ipv4_pattern)) {
        address = boost::asio::ip::make_address_v4(matches[1].str(), error_code);
        port = matches[4].matched ? gsl::narrow_cast<uint16_t>(std::stoul(matches[4].str())) : port;
        return !error_code;
    }
    return false;
}
}  // namespace zenpp
