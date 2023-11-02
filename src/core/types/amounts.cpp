/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "amounts.hpp"

#include <iostream>
#include <regex>

#include <absl/strings/str_cat.h>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

namespace znode {

bool Amount::valid_money() const noexcept { return value_ >= 0 && value_ <= Amount::kMax; }

int64_t Amount::operator*() const noexcept { return value_; }

Amount& Amount::operator=(int64_t value) noexcept {
    value_ = value;
    return *this;
}

Amount& Amount::operator+=(int64_t value) noexcept {
    value_ += value;
    return *this;
}

Amount& Amount::operator*=(int64_t value) noexcept {
    value_ *= value;
    return *this;
}

Amount& Amount::operator-=(int64_t value) noexcept {
    value_ -= value;
    return *this;
}

void Amount::operator++() noexcept { ++value_; }

void Amount::operator--() noexcept { --value_; }

outcome::result<Amount> Amount::from_string(const std::string& input) {
    static const std::string kMaxWhole(std::to_string(kCoinMaxSupply));
    const std::string dyn_pattern{
        boost::str(boost::format(R"(^(\d{0,%i})(\.\d{0,%i})?$)") % kMaxWhole.size() % kCoinMaxDecimals)};
    const std::regex pattern(dyn_pattern, std::regex_constants::icase);

    std::smatch matches;
    if (!std::regex_search(input, matches, pattern, std::regex_constants::match_default)) {
        return boost::system::errc::invalid_argument;
    }

    const std::string whole_part{matches[1].str()};
    std::string fract_part{matches[2].str()};
    if (!fract_part.empty()) {
        fract_part.erase(fract_part.begin());
        fract_part.resize(kCoinMaxDecimals, '0');
    }

    const auto whole{whole_part.empty() ? 0L : boost::lexical_cast<int64_t>(whole_part)};
    const auto fract{fract_part.empty() ? 0L : boost::lexical_cast<int64_t>(fract_part)};
    const auto value{whole * kCoin + fract};
    Amount ret(value);
    if (not ret.valid_money()) return boost::system::errc::result_out_of_range;
    return ret;
}

std::string Amount::to_string() const {
    if (value_ == 0U) return std::string("0 ").append(kCurrency);

    std::string sign;
    auto div{std::div(value_, kCoin)};
    if (value_ < 0) {
        sign.push_back('-');
        div.rem *= -1;
    }

    auto formatted{boost::str(boost::format("%s%i.%08i") % sign % div.quot % div.rem)};

    // Strip all trailing zeroes and also the decimal point if
    // it is the last char of the string.
    char c{formatted.back()};
    while (c not_eq 0x0) {
        switch (c) {
            case '0':
                formatted.pop_back();
                c = formatted.back();
                break;
            case '.':
                formatted.pop_back();
                [[fallthrough]];
            default:
                c = 0;
        }
    }

    return formatted.append(" ").append(kCurrency);
}

std::string FeeRate::to_string() const { return absl::StrCat(Amount::to_string(), "/K"); }

Amount FeeRate::fee(size_t bytes_size) const {
    const auto ret{this->operator*() * static_cast<int64_t>(bytes_size) / static_cast<int64_t>(1_KB)};
    if (ret == 0U) return Amount(this->operator*());
    return Amount(ret);
}
}  // namespace znode
