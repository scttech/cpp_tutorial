#pragma once

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

// An amount of money, stored as a whole number of cents (never as a double: 0.1 + 0.2 != 0.3 in binary).
class Money {
public:
    // No argument: $0.00. The default member initializer below does the work, so `= default` is enough.
    Money() = default;

    // From cents. `explicit`: an int must never turn into Money by accident (`charge(500)` is $5.00? 500 cents? a bug).
    explicit Money(long long cents) : cents_(cents) {}

    // From dollars and cents. It adds no state of its own, so it DELEGATES to the constructor above; the helper
    // validates first, so an invalid amount never becomes a Money.
    Money(long long dollars, int cents) : Money(to_cents(dollars, cents)) {}

    long long cents() const { return cents_; }

    // "$12.34", "-$0.05", "$0.00"
    std::string to_string() const {
        // Work with the magnitude as an unsigned number: negating LLONG_MIN does not fit in a long long.
        const bool negative = cents_ < 0;
        const unsigned long long magnitude =
            negative ? 0ULL - static_cast<unsigned long long>(cents_) : static_cast<unsigned long long>(cents_);
        std::ostringstream out;
        if (negative) out << '-';
        out << '$' << magnitude / 100 << '.' << std::setw(2) << std::setfill('0') << magnitude % 100;
        return out.str();
    }

    Money plus(Money other) const {
        const long long max = std::numeric_limits<long long>::max(), min = std::numeric_limits<long long>::min();
        if ((other.cents_ > 0 && cents_ > max - other.cents_) || (other.cents_ < 0 && cents_ < min - other.cents_)) {
            throw std::overflow_error("Money: the sum does not fit");
        }
        return Money(cents_ + other.cents_);
    }

private:
    // dollars >= 0 and 0 <= cents <= 99, else std::invalid_argument; a total that does not fit: std::overflow_error.
    static long long to_cents(long long dollars, int cents) {
        if (dollars < 0) throw std::invalid_argument("Money: dollars must not be negative");
        if (cents < 0 || cents > 99) throw std::invalid_argument("Money: cents must be between 0 and 99");
        if (dollars > (std::numeric_limits<long long>::max() - cents) / 100) {
            throw std::overflow_error("Money: the amount does not fit");
        }
        return dollars * 100 + cents;
    }

    long long cents_ = 0;
};
