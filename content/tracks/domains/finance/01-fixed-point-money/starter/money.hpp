#pragma once
// YOUR MONEY TYPE: an amount of money as a whole number of CENTS in an int64. No floating point anywhere.
//
//   Money::from_cents(n), cents()        exact
//   ==, <=>                              compare amounts
//   checked_add(a, b), checked_sub(a, b), checked_mul(a, n)     the result, or std::nullopt if it does not fit in an int64
//   negated(), abs()                     std::optional<Money>: nullopt when the result does not fit (-INT64_MIN)
//   divided(n, mode)       the amount divided by the integer n, rounded to a whole cent. nullopt for n == 0 or an overflow.
//   percent(bp, mode)      amount * bp / 10000, rounded to a cent (bp = basis points: 825 is 8.25 %). The multiplication may
//                          not overflow in between: use a wider type (__int128) or check carefully. nullopt if the result does not fit.
//   allocate(parts)        split into `parts` shares that add up EXACTLY: the shares differ by at most one cent, the larger
//                          ones first: 100 cents in 3 gives 34, 33, 33; -100 gives -34, -33, -33. parts < 1: an empty vector.
//   allocate_by_ratio(r)   shares proportional to the non-negative integers r (a total of 0 or a negative ratio: empty vector).
//                          share_i = trunc(amount * r_i / total); the cents left over (still exact in sum) go one each, in order, to
//                          the first shares, in the direction of the sign of the amount.
//   to_string()            "$12.34", "-$0.05", "$0.00": a $, two digits of cents, no separators. Must work for INT64_MIN.
//   Money::parse(text)     an optional '-', an optional '$', one or more digits, then optionally '.' and ONE or TWO digits.
//                          Nothing else (no spaces, no '+', no thousands separators, no ".5", no "5."). "12.3" means 12.30.
//                          nullopt for a bad format or a value outside the int64 cents range ("-$92233720368547758.08" is exactly INT64_MIN).
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace fin {

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

    std::optional<Money> negated() const { return std::nullopt; }  // TODO
    std::optional<Money> abs() const { return std::nullopt; }      // TODO
    std::optional<Money> divided(std::int64_t n, Rounding mode) const {
        (void)n;
        (void)mode;
        return std::nullopt;  // TODO
    }
    std::optional<Money> percent(std::int64_t basis_points, Rounding mode) const {
        (void)basis_points;
        (void)mode;
        return std::nullopt;  // TODO
    }
    std::vector<Money> allocate(int parts) const {
        (void)parts;
        return {};  // TODO
    }
    std::vector<Money> allocate_by_ratio(const std::vector<std::int64_t>& ratios) const {
        (void)ratios;
        return {};  // TODO
    }
    std::string to_string() const { return "$?"; }  // TODO
    static std::optional<Money> parse(std::string_view text) {
        (void)text;
        return std::nullopt;  // TODO
    }

private:
    std::int64_t cents_ = 0;
};

inline std::optional<Money> checked_add(Money a, Money b) {
    (void)a;
    (void)b;
    return std::nullopt;  // TODO
}
inline std::optional<Money> checked_sub(Money a, Money b) {
    (void)a;
    (void)b;
    return std::nullopt;  // TODO
}
inline std::optional<Money> checked_mul(Money a, std::int64_t n) {
    (void)a;
    (void)n;
    return std::nullopt;  // TODO
}

}  // namespace fin
