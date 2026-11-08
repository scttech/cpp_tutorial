// MUTANT: leaves a negative denominator in place.
#pragma once

#include <compare>
#include <numeric>
#include <ostream>
#include <stdexcept>

// A rational number, always stored in lowest terms with a positive denominator, so equal values have
// equal members (that is what makes `==` trivial) and 0 is always 0/1.
class Fraction {
public:
    // Not explicit on purpose: a whole number IS a fraction, so `Fraction f = 3;` and `2 + f` should work.
    Fraction(long long numerator = 0, long long denominator = 1) : num_(numerator), den_(denominator) {
        if (den_ == 0) throw std::invalid_argument("Fraction: the denominator is zero");
        const long long g = std::gcd(num_, den_);  // gcd(0, n) is n, so zero becomes 0/1
        num_ /= g;
        den_ /= g;
    }

    long long numerator() const { return num_; }
    long long denominator() const { return den_; }

    // Explicit on purpose: a silent Fraction -> double conversion would make `f + 2` ambiguous and hide
    // precision loss.
    explicit operator double() const { return static_cast<double>(num_) / static_cast<double>(den_); }

    Fraction operator-() const { return Fraction(-num_, den_); }

    Fraction& operator+=(const Fraction& o) {
        // Use the least common denominator, not den_ * o.den_: it overflows much later.
        const long long g = std::gcd(den_, o.den_);
        *this = Fraction(num_ * (o.den_ / g) + o.num_ * (den_ / g), (den_ / g) * o.den_);
        return *this;
    }
    Fraction& operator-=(const Fraction& o) { return *this += -o; }
    Fraction& operator*=(const Fraction& o) {
        // Cancel across before multiplying, so the products stay as small as possible.
        const long long g1 = std::gcd(num_, o.den_), g2 = std::gcd(o.num_, den_);
        *this = Fraction((num_ / g1) * (o.num_ / g2), (den_ / g2) * (o.den_ / g1));
        return *this;
    }
    Fraction& operator/=(const Fraction& o) {
        if (o.num_ == 0) throw std::domain_error("Fraction: division by zero");
        return *this *= Fraction(o.den_, o.num_);
    }

    // Both are friends defined here, so they are found through either operand and `2 == f` works too.
    friend bool operator==(const Fraction&, const Fraction&) = default;  // lowest terms: same value, same members
    friend std::strong_ordering operator<=>(const Fraction& a, const Fraction& b) {
        return a.num_ * b.den_ <=> b.num_ * a.den_;  // denominators are positive, so the order is preserved
    }

private:
    long long num_;
    long long den_;
};

// Binary operators are NOT members: that way both operands may be converted, so `2 + f` works as well as `f + 2`.
inline Fraction operator+(Fraction a, const Fraction& b) { return a += b; }
inline Fraction operator-(Fraction a, const Fraction& b) { return a -= b; }
inline Fraction operator*(Fraction a, const Fraction& b) { return a *= b; }
inline Fraction operator/(Fraction a, const Fraction& b) { return a /= b; }

// 3/4, -3/4, and whole numbers without the "/1": 2
inline std::ostream& operator<<(std::ostream& os, const Fraction& f) {
    os << f.numerator();
    if (f.denominator() != 1) os << '/' << f.denominator();
    return os;
}
