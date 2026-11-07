// MUTANT: checks AFTER adding. Signed overflow is undefined behavior, and this only handles one direction.
#include <cstdint>

bool safe_add(int a, int b, int& out) {
    int r = a + b;
    if (b > 0 && r < a) return false;
    out = r;
    return true;
}
int floor_mod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }
bool is_power_of_two(unsigned int x) { return x != 0 && (x & (x - 1)) == 0; }
int count_set_bits(std::uint32_t x) { int c = 0; while (x) { x &= x - 1; ++c; } return c; }
