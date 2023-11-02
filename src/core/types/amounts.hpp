/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <core/common/base.hpp>

namespace znode {

//! \brief Type-Safe wrapper class for token amounts
class Amount {
  public:
    Amount() = default;
    virtual ~Amount() = default;
    explicit Amount(int64_t value) : value_{value} {};

    static constexpr int64_t kMax{kCoinMaxSupply * kCoin};

    //! \brief Returns wether the amount value is in valid range (0, kMax)
    [[nodiscard]] bool valid_money() const noexcept;

    explicit operator bool() const noexcept { return value_ != 0; }
    auto operator<=>(const Amount& rhs) const noexcept = default;

    [[nodiscard]] int64_t operator*() const noexcept;
    Amount& operator=(int64_t value) noexcept;
    Amount& operator+=(int64_t value) noexcept;
    Amount& operator*=(int64_t value) noexcept;
    Amount& operator-=(int64_t value) noexcept;
    void operator++() noexcept;
    void operator--() noexcept;

    //! \brief Parses an amount expressed in token denomination (e.g. 1.0458)
    //! \remarks Should the input not match the boundaries of Amount or not honors the valid_money() test
    //! then an error is returned
    static outcome::result<Amount> from_string(const std::string& input);

    //! \brief Returns the string representation of Amount expressed in token denomination (e.g. 1.0458)
    [[nodiscard]] virtual std::string to_string() const;

    friend Amount operator+(const Amount& lhs, const Amount& rhs) { return Amount(lhs.value_ + rhs.value_); }
    friend Amount operator-(const Amount& lhs, const Amount& rhs) { return Amount(lhs.value_ - rhs.value_); }
    friend Amount operator*(const Amount& lhs, const Amount& rhs) { return Amount(lhs.value_ * rhs.value_); }
    friend Amount operator/(const Amount& lhs, const Amount& rhs) { return Amount(lhs.value_ / rhs.value_); }
    friend Amount operator%(const Amount& lhs, const Amount& rhs) { return Amount(lhs.value_ % rhs.value_); }

  private:
    int64_t value_{0};
};

inline bool operator==(const Amount& lhs, int64_t value) noexcept { return *lhs == value; }
inline auto operator<=>(const Amount& lhs, int64_t value) noexcept { return *lhs <=> value; }

//! \brief Type-safe wrapper class to for fee rates i.e. how much a transaction pays for inclusion
//! \remarks The fee rate is expressed in Amount per 1'000 bytes (not 1024)
class FeeRate : public Amount {
  public:
    using Amount::Amount;
    ~FeeRate() override = default;

    FeeRate(const Amount& paid, size_t size)
        : Amount(size ? *paid * static_cast<int64_t>(1_KB) / static_cast<int64_t>(size) : int64_t(0)){};

    [[nodiscard]] Amount fee(size_t bytes_size = 1_KB) const;
    [[nodiscard]] std::string to_string() const override;

    auto operator<=>(const FeeRate& rhs) const noexcept = default;
};

}  // namespace znode
