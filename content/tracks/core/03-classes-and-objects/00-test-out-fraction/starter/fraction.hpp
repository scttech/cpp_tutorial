#pragma once

#include <compare>
#include <numeric>
#include <ostream>
#include <stdexcept>

// A rational number in lowest terms with a positive denominator. Every TODO below is part of the exercise.
class Fraction {
public:
    // TODO: throw std::invalid_argument if the denominator is 0; keep the denominator positive (move the sign to the
    // numerator); reduce to lowest terms (std::gcd). Fraction() and Fraction(0, 5) are both 0/1.
    Fraction(long long numerator = 0, long long denominator = 1) : num_(numerator), den_(denominator) {}

    long long numerator() const { return num_; }
    long long denominator() const { return den_; }

    // TODO: the value as a double. `explicit`: it must only happen when asked for with static_cast<double>.
    explicit operator double() const { return 0.0; }

    // TODO: negation
    Fraction operator-() const { return *this; }

    // TODO: the four compound assignments. Keep the numbers small: use the least common denominator when adding
    // and cancel before multiplying. Dividing by a zero fraction throws std::domain_error.
    Fraction& operator+=(const Fraction& o) { (void)o; return *this; }
    Fraction& operator-=(const Fraction& o) { (void)o; return *this; }
    Fraction& operator*=(const Fraction& o) { (void)o; return *this; }
    Fraction& operator/=(const Fraction& o) { (void)o; return *this; }

    // TODO: equality and ordering. With a normalized representation, `==` can compare the members.
    friend bool operator==(const Fraction&, const Fraction&) = default;
    friend std::strong_ordering operator<=>(const Fraction& a, const Fraction& b) {
        (void)a;
        (void)b;
        return std::strong_ordering::equal;
    }

private:
    long long num_;
    long long den_;
};

// TODO: the four binary operators, built from the compound ones. Not members: `2 + f` must work as well as `f + 2`.
inline Fraction operator+(const Fraction& a, const Fraction& b) { (void)b; return a; }
inline Fraction operator-(const Fraction& a, const Fraction& b) { (void)b; return a; }
inline Fraction operator*(const Fraction& a, const Fraction& b) { (void)b; return a; }
inline Fraction operator/(const Fraction& a, const Fraction& b) { (void)b; return a; }

// TODO: print 3/4 or -3/4; a whole number prints without the denominator: 2, not 2/1.
inline std::ostream& operator<<(std::ostream& os, const Fraction& f) {
    (void)f;
    return os;
}
