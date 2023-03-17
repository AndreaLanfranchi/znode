/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/encoding/errors.hpp>

namespace zen {

//! \brief Type-Safe wrapper class for token amounts
class Amount {
  public:
    Amount() = default;
    explicit Amount(int64_t value) : amount_{value} {};

    Amount(const Amount& other) = default;

    static constexpr int64_t kMax{kCoinMaxSupply * kCoin};

    //! \brief Parses an amount expressed in token denomination (e.g. 1.0458)
    //! \remarks Should the input not match the boundaries of Amount or not honors the valid_money() test
    //! then an unexpected DecodingError is returned
    static tl::expected<Amount, DecodingError> parse(const std::string& input);

    [[nodiscard]] bool valid_money() const noexcept;
    explicit operator bool() const noexcept;
    bool operator==(int64_t value) const noexcept;
    bool operator>(int64_t value) const noexcept;
    bool operator<(int64_t value) const noexcept;
    bool operator==(const Amount& rhs) const noexcept = default;
    friend auto operator<=>(const Amount& lhs, const Amount& rhs) = default;

    [[nodiscard]] int64_t operator*() const noexcept;
    Amount& operator=(int64_t value) noexcept;
    Amount& operator=(const Amount& rhs) noexcept;
    void operator+=(int64_t value) noexcept;
    void operator*=(int64_t value) noexcept;
    void operator-=(int64_t value) noexcept;
    void operator++() noexcept;
    void operator--() noexcept;

    [[nodiscard]] std::string to_string() const;

    friend Amount operator+(const Amount& lhs, const Amount& rhs) { return Amount(lhs.amount_ + rhs.amount_); }
    friend Amount operator-(const Amount& lhs, const Amount& rhs) { return Amount(lhs.amount_ - rhs.amount_); }
    friend Amount operator*(const Amount& lhs, const Amount& rhs) { return Amount(lhs.amount_ * rhs.amount_); }
    friend Amount operator/(const Amount& lhs, const Amount& rhs) { return Amount(lhs.amount_ / rhs.amount_); }
    friend Amount operator%(const Amount& lhs, const Amount& rhs) { return Amount(lhs.amount_ % rhs.amount_); }

  private:
    int64_t amount_{0};
};

//! \brief Type-safe wrapper class to for fee rates i.e. how much a transaction pays for inclusion
class FeeRate {
  public:
    FeeRate() = default;
    explicit FeeRate(const int64_t value) : satoshis_per_K_(value) {}
    explicit FeeRate(const Amount amount) : satoshis_per_K_(amount) {}
    FeeRate(Amount paid, size_t size);

    FeeRate(const FeeRate& other) = default;
    FeeRate(const FeeRate&& other) noexcept : satoshis_per_K_{other.satoshis_per_K_} {};

    [[nodiscard]] Amount fee(size_t bytes_size = 1'000) const;

    [[nodiscard]] std::string to_string() const;

    friend auto operator<=>(const FeeRate& lhs, const FeeRate& rhs) = default;

  private:
    Amount satoshis_per_K_{0};
};

}  // namespace zen
