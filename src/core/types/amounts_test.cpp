/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <boost/format.hpp>
#include <catch2/catch.hpp>

#include <zen/core/types/amounts.hpp>

namespace zen {

std::string append_currency(const std::string_view& input) {
    return boost::str(boost::format("%s %s") % input % kCurrency);
}

TEST_CASE("Amounts", "[types]") {
    Amount a1;
    CHECK_FALSE(a1);
    CHECK(a1.to_string() == append_currency("0"));

    a1 += Amount::kMax + 1;
    CHECK(a1);
    CHECK_FALSE(a1.valid_money());

    a1 = 10;
    CHECK(a1);
    CHECK(a1.valid_money());
    a1 = -2;
    CHECK(a1);
    CHECK_FALSE(a1.valid_money());

    Amount a2{a1};
    CHECK(a1 == a2);
    CHECK_FALSE(a1 > a2);
    CHECK_FALSE(a1 < a2);

    ++a1;
    CHECK(a1 >= a2);
    --a1;
    CHECK(a1 == a2);
    auto a3{a1 + a2};
    CHECK_FALSE(a3.valid_money());
    CHECK(a3.to_string() == append_currency("-0.00000004"));

    a3 *= -1;
    CHECK(a3.to_string() == append_currency("0.00000004"));

    a1 = 1'000'000;
    CHECK(a1.to_string() == append_currency("0.01"));

    auto parsed{Amount::parse("1.25")};
    CHECK(parsed);
    CHECK(parsed->to_string() == append_currency("1.25"));

    std::string input{std::to_string(kCoinMaxSupply)};
    input.push_back('0');  // To exceed max allowable length
    parsed = Amount::parse(input);
    CHECK_FALSE(parsed);
    CHECK(parsed.error() == DecodingError::kInvalidInput);
    input.pop_back();
    parsed = Amount::parse(input);
    CHECK((parsed && *parsed == Amount::kMax));

    input = std::to_string(kCoinMaxSupply + 2);  // To cause overflow on range
    parsed = Amount::parse(input);
    CHECK_FALSE(parsed);
    CHECK(parsed.error() == DecodingError::kInvalidAmountRange);

    std::string decimals(kCoinMaxDecimals, '1');
    decimals.push_back('1');  // Exceed the amount of allowed digits
    parsed = Amount::parse("10." + decimals);
    CHECK_FALSE(parsed);
    decimals.pop_back();
    parsed = Amount::parse("10." + decimals);
    CHECK(parsed);
}

TEST_CASE("FeeRates", "[types]") {
    FeeRate fr1{10};
    CHECK(fr1.to_string() == append_currency("0.0000001").append("/K"));

    FeeRate fr2{1520 * kCoin};
    CHECK(fr2.to_string() == append_currency("1520").append("/K"));
    CHECK(fr1 != fr2);

    // Nominal fee
    auto fee{fr2.fee()};
    CHECK(fee == 1520 * kCoin);

    // Fee for specific size
    fee = fr2.fee(100);
    CHECK(fee == 1520 * kCoin / 10);
}
}  // namespace zen
