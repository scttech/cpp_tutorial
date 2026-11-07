#include <cstdint>
#include <iostream>
#include <limits>

// If a + b fits in an int, store it in `out` and return true.
// Otherwise return false and leave `out` untouched. Do not overflow while checking!
bool safe_add(int a, int b, int& out) {
    // TODO
    (void)a;
    (void)b;
    (void)out;
    return false;
}

// For m > 0: the remainder of a / m in the range [0, m), even for negative a.
int floor_mod(int a, int m) {
    // TODO
    (void)a;
    (void)m;
    return 0;
}

// True for 1, 2, 4, 8, ...; false for 0 and everything else. Use a bitwise trick.
bool is_power_of_two(unsigned int x) {
    // TODO
    (void)x;
    return false;
}

// The number of 1 bits in x.
int count_set_bits(std::uint32_t x) {
    // TODO
    (void)x;
    return 0;
}

int main() {
    int sum = 0;
    const int big = std::numeric_limits<int>::max();
    std::cout << "2 + 3 fits: " << safe_add(2, 3, sum) << " (sum = " << sum << ")\n";
    std::cout << "INT_MAX + 1 fits: " << safe_add(big, 1, sum) << "\n";
    std::cout << "floor_mod(-1, 5) = " << floor_mod(-1, 5) << "\n";
    std::cout << "is_power_of_two(64) = " << is_power_of_two(64) << "\n";
    std::cout << "count_set_bits(255) = " << count_set_bits(255) << "\n";
}
