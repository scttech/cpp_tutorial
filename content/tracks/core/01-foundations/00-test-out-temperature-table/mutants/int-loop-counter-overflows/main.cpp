// MUTANT: int loop counter: c += step overflows (UB) near INT_MAX and the loop never ends.
#include <iomanip>
#include <iostream>

int main() {
    int start = 0;
    int end = 0;
    int step = 0;
    if (!(std::cin >> start >> end >> step)) {
        std::cout << "error: expected three integers: start end step\n";
        return 0;
    }
    if (step <= 0) {
        std::cout << "error: step must be positive\n";
        return 0;
    }
    if (start > end) {
        std::cout << "error: start must not exceed end\n";
        return 0;
    }

    std::cout << std::setw(8) << "Celsius" << std::setw(12) << "Fahrenheit" << '\n';
    std::cout << std::fixed << std::setprecision(1);
    // long long: `c += step` cannot overflow int, even when end is near INT_MAX
    for (int c = start; c <= end; c += step) {
        const double fahrenheit = static_cast<double>(c) * 9.0 / 5.0 + 32.0;
        std::cout << std::setw(8) << c << std::setw(12) << fahrenheit << '\n';
    }
    return 0;
}
