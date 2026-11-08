// A small demo of your Fraction class (read-only). Press Run to see what it prints.
#include <iostream>

#include "fraction.hpp"

int main() {
    const Fraction a(1, 2), b(3, 4);
    std::cout << a << " + " << b << " = " << a + b << "\n";
    std::cout << a << " - " << b << " = " << a - b << "\n";
    std::cout << a << " * " << b << " = " << a * b << "\n";
    std::cout << a << " / " << b << " = " << a / b << "\n";
    std::cout << "6/8 reduces to " << Fraction(6, 8) << ", and 1/-2 is " << Fraction(1, -2) << "\n";
    std::cout << (a < b ? "1/2 is smaller than 3/4" : "1/2 is NOT smaller than 3/4") << "\n";
}
