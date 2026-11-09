// A demo of metrics.hpp (read-only). It does not build until every warning in metrics.hpp is fixed: this lesson compiles with
// -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror, so each warning is an error.
#include <iostream>

#include "metrics.hpp"

int main() {
    Counter counter(5);
    counter.add(3);
    std::cout << "counter: " << counter.value() << "\n";
    std::cout << "sum of squares of 1 2 3: " << sum_of_squares({1, 2, 3}) << "\n";
    std::cout << "73.9% as a whole number: " << percent(0.739) << "\n";
    std::cout << "clamp_to_byte(300): " << static_cast<int>(clamp_to_byte(300)) << "\n";
    std::cout << "pad_left(\"7\", 3, '0'): " << pad_left("7", 3, '0') << "\n";
    std::cout << "grade_points(85): " << grade_points(85) << ", grade_points(40): " << grade_points(40) << "\n";
    std::cout << "label(High): " << label(Level::High) << ", label(Critical): " << label(Level::Critical) << "\n";
    std::cout << "is_zero(0): " << is_zero(0) << ", is_zero(7): " << is_zero(7) << "\n";
    std::cout << "sum_to(100): " << sum_to(100) << "\n";
}
