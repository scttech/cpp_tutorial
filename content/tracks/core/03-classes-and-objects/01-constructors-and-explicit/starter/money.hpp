#pragma once

#include <string>

// An amount of money, stored as a whole number of cents. Every TODO below is part of the exercise.
class Money {
public:
    // TODO: no argument means $0.00. Prefer `= default` together with a default member initializer on cents_.
    Money() {}

    // TODO: this converts any number to Money silently. Make it `explicit`.
    Money(long long cents) : cents_(cents) {}

    // TODO: validate (see the lesson) and DELEGATE to the constructor above instead of repeating its work.
    Money(long long dollars, int cents) : cents_(dollars * 100 + cents) {}

    long long cents() const { return cents_; }

    // TODO: "$12.34", "-$0.05", "$0.00"
    std::string to_string() const { return ""; }

    // TODO: the sum; throw std::overflow_error if it does not fit in a long long.
    Money plus(Money other) const {
        (void)other;
        return *this;
    }

private:
    long long cents_;
};
