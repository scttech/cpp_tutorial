// A demo of your generic functions (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>
#include <vector>

#include "generic.hpp"

int main() {
    std::cout << "clamp_value(15, 0, 10) = " << clamp_value(15, 0, 10) << ", clamp_value(-3, 0, 10) = " << clamp_value(-3, 0, 10) << "\n";
    std::cout << "clamp_value(0.5, 0.0, 1.0) = " << clamp_value(0.5, 0.0, 1.0) << "\n";

    std::cout << "min_of(3, 7) = " << min_of(3, 7) << "\n";

    int a = 1, b = 2;
    swap_values(a, b);
    std::cout << "after swap_values: a = " << a << ", b = " << b << "\n";

    // Once the functions are templates these work too (try them):
    //   std::cout << min_of({4, 2, 8}) << "\n";
    //   std::cout << min_of(2, 1.5) << "\n";   (an int and a double: 1.5, not 1)
    //   std::string x = "left", y = "right"; swap_values(x, y);
    //   clamp_value(std::string("m"), std::string("a"), std::string("f"))
    //   clamp_value(5, 0, 3.5)   <- an ERROR: T is deduced as int from the first two arguments and as double from the third
}
