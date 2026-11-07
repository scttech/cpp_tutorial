// MUTANT: forgets that 0 is not a power of two ((0 & (0 - 1)) == 0 is true).
#include <climits>
#include <cstdint>

bool safe_add(int a, int b, int& out) {
    if (b > 0 && a > INT_MAX - b) return false;
    if (b < 0 && a < INT_MIN - b) return false;
    out = a + b;
    return true;
}
int floor_mod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }
bool is_power_of_two(unsigned int x) { return (x & (x - 1)) == 0; }
int count_set_bits(std::uint32_t x) { int c = 0; while (x) { x &= x - 1; ++c; } return c; }
