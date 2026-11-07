#include <cstdint>
#include <iostream>
#include <limits>

bool safe_add(int a, int b, int& out) {
    constexpr int kMax = std::numeric_limits<int>::max();
    constexpr int kMin = std::numeric_limits<int>::min();
    if (b > 0 && a > kMax - b) return false;  // would exceed INT_MAX
    if (b < 0 && a < kMin - b) return false;  // would go below INT_MIN
    out = a + b;
    return true;
}

int floor_mod(int a, int m) {
    const int r = a % m;
    return r < 0 ? r + m : r;
}

bool is_power_of_two(unsigned int x) {
    return x != 0 && (x & (x - 1)) == 0;
}

int count_set_bits(std::uint32_t x) {
    int count = 0;
    while (x != 0) {
        x &= x - 1;  // clear the lowest set bit
        ++count;
    }
    return count;
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
