// A small demo of your Money class (read-only). Press Run to see what it prints.
#include <iostream>
#include <stdexcept>

#include "money.hpp"

void charge(Money price) { std::cout << "charging " << price.to_string() << "\n"; }

int main() {
    Money nothing;  // if this prints garbage, the default constructor leaves cents_ uninitialized
    const Money from_cents(1234);
    const Money from_parts(12, 34);
    std::cout << "default: " << nothing.to_string() << "\n";
    std::cout << "1234 cents: " << from_cents.to_string() << "\n";
    std::cout << "12 dollars 34 cents: " << from_parts.to_string() << "\n";
    std::cout << "a debt of 5 cents: " << Money(-5).to_string() << "\n";
    std::cout << "sum: " << from_cents.plus(Money(66)).to_string() << "\n";

    charge(Money(500));
    // charge(500);  // with an explicit constructor this does NOT compile: that is the point of `explicit`.

    try {
        const Money bad(1, 100);  // 100 cents is not a valid "cents" part
        std::cout << "accepted " << bad.to_string() << "\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "rejected: " << e.what() << "\n";
    }
}
