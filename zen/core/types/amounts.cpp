/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <iostream>
#include <regex>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include <zen/core/types/amounts.hpp>

namespace zen {

bool Amount::valid_money() const noexcept { return amount_ >= 0 && amount_ <= Amount::kMax; }

Amount::operator bool() const noexcept { return amount_ != 0; }

bool Amount::operator==(int64_t value) const noexcept { return amount_ == value; }

bool Amount::operator>(int64_t value) const noexcept { return amount_ > value; }

bool Amount::operator<(int64_t value) const noexcept { return amount_ < value; }

int64_t Amount::operator*() const noexcept { return amount_; }

Amount& Amount::operator=(int64_t value) noexcept {
    amount_ = value;
    return *this;
}

Amount& Amount::operator=(const Amount& rhs) noexcept {
    amount_ = *rhs;
    return *this;
}

std::string Amount::to_string() const {
    if (!amount_) return std::string("0 ").append(kCurrency);

    std::string sign;
    auto div{std::div(amount_, kCoin)};
    if (amount_ < 0) {
        sign.push_back('-');
        div.rem *= -1;
    }

    auto formatted{boost::str(boost::format("%s%i.%08i") % sign % div.quot % div.rem)};

    // Strip all trailing zeroes and also the decimal point if
    // it is the last char of the string.
    char c{formatted.back()};
    while (c) {
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

void Amount::operator+=(int64_t value) noexcept { amount_ += value; }

void Amount::operator*=(int64_t value) noexcept { amount_ *= value; }

void Amount::operator-=(int64_t value) noexcept { amount_ -= value; }

void Amount::operator++() noexcept { ++amount_; }

void Amount::operator--() noexcept { --amount_; }

tl::expected<Amount, DecodingError> Amount::parse(const std::string& input) {
    static const std::string kMaxWhole(std::to_string(kCoinMaxSupply));
    const std::string dyn_pattern{
        boost::str(boost::format("^(\\d{0,%i})(\\.\\d{0,%i})?$") % kMaxWhole.size() % kCoinMaxDecimals)};
    const std::regex pattern(dyn_pattern, std::regex_constants::icase);

    std::smatch matches;
    if (!std::regex_search(input, matches, pattern, std::regex_constants::match_default)) {
        return tl::unexpected{DecodingError::kInvalidInput};
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
    if (!ret.valid_money()) return tl::unexpected{DecodingError::kInvalidAmountRange};
    return Amount(value);
}

FeeRate::FeeRate(const Amount paid, size_t size) {
    satoshis_per_K_ = size ? *paid * 1'000L / static_cast<int64_t>(size) : 0;
}

std::string FeeRate::to_string() const { return satoshis_per_K_.to_string() + "/K"; }
Amount FeeRate::fee(size_t bytes_size) const {
    Amount ret(*satoshis_per_K_ * static_cast<int64_t>(bytes_size) / 1'000);
    if (!ret && satoshis_per_K_) ret = satoshis_per_K_;
    return ret;
}
}  // namespace zen
