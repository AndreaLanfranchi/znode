/*
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

#include <format>

#include <catch2/catch.hpp>

#include <core/types/amounts.hpp>

namespace znode {

std::string append_currency(const std::string_view& input) { return std::format("{} {}", input, kCurrency); }

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

    auto parsed{Amount::from_string("1.25")};
    REQUIRE(parsed);
    CHECK(parsed.value().to_string() == append_currency("1.25"));

    std::string input{std::to_string(kCoinMaxSupply)};
    input.push_back('0');  // To exceed max allowable length
    parsed = Amount::from_string(input);
    CHECK_FALSE(parsed);
    CHECK(parsed.error() == boost::system::errc::invalid_argument);
    input.pop_back();
    parsed = Amount::from_string(input);
    REQUIRE(parsed);
    CHECK(parsed.value() == Amount::kMax);

    input = std::to_string(kCoinMaxSupply + 2);  // To cause overflow on range
    parsed = Amount::from_string(input);
    CHECK_FALSE(parsed);
    CHECK(parsed.error().value() == static_cast<int>(boost::system::errc::result_out_of_range));

    std::string decimals(kCoinMaxDecimals, '1');
    decimals.push_back('1');  // Exceed the amount of allowed digits
    parsed = Amount::from_string("10." + decimals);
    CHECK_FALSE(parsed);
    decimals.pop_back();
    parsed = Amount::from_string("10." + decimals);
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
}  // namespace znode
