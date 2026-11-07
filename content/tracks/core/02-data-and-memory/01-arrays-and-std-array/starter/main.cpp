#include <array>
#include <cstddef>
#include <iostream>

#include "matrix.hpp"

// The transpose swaps rows and columns: result[c][r] == m[r][c].
Matrix3x2 transpose(const Matrix2x3& m) {
    // TODO
    (void)m;
    return {};
}

// Sum of `count` ints starting at `data`. Returns long long so that big values do not overflow.
// A plain C array decays to a pointer when passed, so the length has to travel separately.
long long sum_range(const int* data, std::size_t count) {
    // TODO
    (void)data;
    (void)count;
    return 0;
}

// a[index] if index is inside the array, otherwise `fallback`. (operator[] never checks for you.)
int at_or(const std::array<int, 6>& a, std::size_t index, int fallback) {
    // TODO
    (void)a;
    (void)index;
    return fallback;
}

// The position of the first element equal to `value`, or a.size() if there is none.
std::size_t index_of(const std::array<int, 6>& a, int value) {
    // TODO
    (void)a;
    (void)value;
    return 0;
}

// What does the function see? Run this and compare the two sizes.
static std::size_t bytes_seen_by_callee(const int* p) {
    return sizeof(p);  // the size of a pointer, not of the array it points into
}

int main() {
    const int raw[4] = {10, 20, 30, 40};
    std::cout << "the array is " << sizeof(raw) << " bytes; the callee sees " << bytes_seen_by_callee(raw) << " bytes\n";
    std::cout << "sum_range = " << sum_range(raw, 4) << "\n";

    const Matrix2x3 m{{{1, 2, 3}, {4, 5, 6}}};
    const Matrix3x2 t = transpose(m);
    for (const auto& row : t) std::cout << row[0] << ' ' << row[1] << '\n';
}
