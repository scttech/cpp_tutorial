// MUTANT: plain % keeps the dividend's sign, so floor_mod(-1, 5) is -1.
#include <climits>
#include <cstdint>

bool safe_add(int a, int b, int& out) {
    if (b > 0 && a > INT_MAX - b) return false;
    if (b < 0 && a < INT_MIN - b) return false;
    out = a + b;
    return true;
}
int floor_mod(int a, int m) { return a % m; }
bool is_power_of_two(unsigned int x) { return x != 0 && (x & (x - 1)) == 0; }
int count_set_bits(std::uint32_t x) { int c = 0; while (x) { x &= x - 1; ++c; } return c; }
