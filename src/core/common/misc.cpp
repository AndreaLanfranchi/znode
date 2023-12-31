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

#include "misc.hpp"

#include <array>
#include <regex>
#include <set>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/format.hpp>
#include <gsl/gsl_util>

#include <core/common/random.hpp>

namespace znode {

std::string abridge(std::string_view input, size_t length) {
    if (input.length() <= length) return std::string(input);
    std::string abridged{input.substr(0, length)};
    boost::algorithm::trim_right(abridged);
    abridged += "...";
    return abridged;
}

outcome::result<uint64_t> parse_human_bytes(const std::string& input) {
    if (input.empty()) {
        return 0ULL;
    }

    static const std::regex pattern{R"(^(\d{0,10})(\.\d{1,3})?\ *?(B|KB|MB|GB|TB|KiB|MiB|GiB|TiB)?$)",
                                    std::regex_constants::icase};
    std::smatch matches;
    if (!std::regex_search(input, matches, pattern, std::regex_constants::match_default)) {
        return boost::system::errc::invalid_argument;
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
}  // namespace znode
