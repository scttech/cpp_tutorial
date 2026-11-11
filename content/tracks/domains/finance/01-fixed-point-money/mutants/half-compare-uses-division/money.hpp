// MUTANT: the half test uses the truncated half of the divisor (wrong for odd divisors)
#pragma once
// Money as int64 cents. Every operation either gives the exact result or says (nullopt) that it cannot.
#include <charconv>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace fin {

namespace detail {

using Wide = __int128;

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

constexpr std::optional<std::int64_t> narrow(Wide v) {
    if (v > kMax || v < kMin) return std::nullopt;
    return static_cast<std::int64_t>(v);
}

// num / den (den != 0) rounded to a whole number
constexpr Wide divide_rounded(Wide num, Wide den, Rounding mode) {
    Wide q = num / den;  // truncates toward zero
    const Wide r = num % den;
    if (r == 0 || mode == Rounding::Down) return q;
    const Wide twice = (r < 0 ? -r : r);
    const Wide mag = (den < 0 ? -den : den) / 2;
    const bool away = twice > mag || (twice == mag && (mode == Rounding::HalfUp || (q % 2 != 0)));
    if (away) q += ((num < 0) != (den < 0)) ? -1 : 1;  // one step away from zero
    return q;
}

}  // namespace detail

class Money {
public:
    constexpr Money() = default;
    static constexpr Money from_cents(std::int64_t cents) {
        Money m;
        m.cents_ = cents;
        return m;
    }
    constexpr std::int64_t cents() const { return cents_; }

    friend constexpr bool operator==(Money a, Money b) { return a.cents_ == b.cents_; }
    friend constexpr std::strong_ordering operator<=>(Money a, Money b) { return a.cents_ <=> b.cents_; }

    constexpr std::optional<Money> negated() const {
        if (cents_ == detail::kMin) return std::nullopt;
        return from_cents(-cents_);
    }
    constexpr std::optional<Money> abs() const { return cents_ < 0 ? negated() : std::optional<Money>(*this); }

    constexpr std::optional<Money> divided(std::int64_t n, Rounding mode) const {
        if (n == 0) return std::nullopt;
        return wrap(detail::divide_rounded(cents_, n, mode));  // done in 128 bits: INT64_MIN / -1 does not trap
    }

    constexpr std::optional<Money> percent(std::int64_t basis_points, Rounding mode) const {
        return wrap(detail::divide_rounded(static_cast<detail::Wide>(cents_) * basis_points, 10000, mode));
    }

    std::vector<Money> allocate(int parts) const {
        if (parts < 1) return {};
        std::vector<Money> shares;
        shares.reserve(static_cast<std::size_t>(parts));
        const detail::Wide n = parts;
        const detail::Wide base = cents_ / n;                    // toward zero
        detail::Wide left = static_cast<detail::Wide>(cents_) - base * n;  // |left| < parts, same sign as the amount
        const detail::Wide step = cents_ < 0 ? -1 : 1;
        for (int i = 0; i < parts; ++i) {
            detail::Wide share = base;
            if (left != 0) {
                share += step;
                left -= step;
            }
            shares.push_back(from_cents(static_cast<std::int64_t>(share)));
        }
        return shares;
    }

    std::vector<Money> allocate_by_ratio(const std::vector<std::int64_t>& ratios) const {
        detail::Wide total = 0;
        for (const std::int64_t r : ratios) {
            if (r < 0) return {};
            total += r;
        }
        if (ratios.empty() || total == 0) return {};
        std::vector<Money> shares;
        detail::Wide given = 0;
        for (const std::int64_t r : ratios) {
            const detail::Wide share = static_cast<detail::Wide>(cents_) * r / total;
            given += share;
            shares.push_back(from_cents(static_cast<std::int64_t>(share)));
        }
        detail::Wide left = static_cast<detail::Wide>(cents_) - given;
        const std::int64_t step = cents_ < 0 ? -1 : 1;
        for (std::size_t i = 0; left != 0 && i < shares.size(); ++i) {
            shares[i] = from_cents(shares[i].cents_ + step);
            left -= step;
        }
        return shares;
    }

    std::string to_string() const {
        // work with the magnitude as an unsigned number: -INT64_MIN does not fit in a signed one
        const std::uint64_t mag = cents_ < 0 ? 0 - static_cast<std::uint64_t>(cents_) : static_cast<std::uint64_t>(cents_);
        const std::uint64_t units = mag / 100, cents = mag % 100;
        std::string out;
        if (cents_ < 0) out += '-';
        out += '$';
        out += std::to_string(units);
        out += '.';
        out += static_cast<char>('0' + cents / 10);
        out += static_cast<char>('0' + cents % 10);
        return out;
    }

    static std::optional<Money> parse(std::string_view text) {
        std::size_t i = 0;
        bool negative = false;
        if (i < text.size() && text[i] == '-') {
            negative = true;
            ++i;
        }
        if (i < text.size() && text[i] == '$') ++i;
        const std::size_t digits_at = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
        if (i == digits_at) return std::nullopt;  // no digits before the point
        std::uint64_t units = 0;
        {
            const auto result = std::from_chars(text.data() + digits_at, text.data() + i, units);
            if (result.ec != std::errc()) return std::nullopt;  // too many digits for 64 bits
        }
        std::uint64_t cents = 0;
        if (i < text.size()) {
            if (text[i] != '.') return std::nullopt;
            ++i;
            const std::size_t frac_at = i;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
            const std::size_t frac_len = i - frac_at;
            if (frac_len < 1 || frac_len > 2 || i != text.size()) return std::nullopt;
            cents = static_cast<std::uint64_t>(text[frac_at] - '0') * (frac_len == 2 ? 10 : 100) / (frac_len == 2 ? 1 : 10);
            cents = frac_len == 2 ? static_cast<std::uint64_t>(text[frac_at] - '0') * 10 + static_cast<std::uint64_t>(text[frac_at + 1] - '0') : static_cast<std::uint64_t>(text[frac_at] - '0') * 10;
        }
        const detail::Wide total = static_cast<detail::Wide>(units) * 100 + cents;
        const detail::Wide signed_total = negative ? -total : total;
        return wrap(signed_total);
    }

private:
    std::int64_t cents_ = 0;

    static constexpr std::optional<Money> wrap(detail::Wide v) {
        const auto n = detail::narrow(v);
        if (!n) return std::nullopt;
        return from_cents(*n);
    }
    friend constexpr std::optional<Money> checked_add(Money a, Money b);
    friend constexpr std::optional<Money> checked_sub(Money a, Money b);
    friend constexpr std::optional<Money> checked_mul(Money a, std::int64_t n);
};

constexpr std::optional<Money> checked_add(Money a, Money b) { return Money::wrap(static_cast<detail::Wide>(a.cents_) + b.cents_); }
constexpr std::optional<Money> checked_sub(Money a, Money b) { return Money::wrap(static_cast<detail::Wide>(a.cents_) - b.cents_); }
constexpr std::optional<Money> checked_mul(Money a, std::int64_t n) { return Money::wrap(static_cast<detail::Wide>(a.cents_) * n); }

}  // namespace fin
