// MUTANT: compares as int, so a huge unsigned index (-1) looks negative and passes the check.
#include <array>
#include <cstddef>
#include <iostream>

#include "matrix.hpp"

Matrix3x2 transpose(const Matrix2x3& m) {
    Matrix3x2 result{};
    for (std::size_t r = 0; r < m.size(); ++r) {
        for (std::size_t c = 0; c < m[r].size(); ++c) {
            result[c][r] = m[r][c];
        }
    }
    return result;
}

long long sum_range(const int* data, std::size_t count) {
    long long total = 0;  // long long: INT_MAX + INT_MAX does not fit in an int
    for (std::size_t i = 0; i < count; ++i) total += data[i];
    return total;
}

int at_or(const std::array<int, 6>& a, std::size_t index, int fallback) {
    if (static_cast<int>(index) >= static_cast<int>(a.size())) return fallback;
    return a[index];
}

std::size_t index_of(const std::array<int, 6>& a, int value) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] == value) return i;
    }
    return a.size();
}

static std::size_t bytes_seen_by_callee(const int* p) {
    return sizeof(p);
}

int main() {
    const int raw[4] = {10, 20, 30, 40};
    std::cout << "the array is " << sizeof(raw) << " bytes; the callee sees " << bytes_seen_by_callee(raw) << " bytes\n";
    std::cout << "sum_range = " << sum_range(raw, 4) << "\n";

    const Matrix2x3 m{{{1, 2, 3}, {4, 5, 6}}};
    const Matrix3x2 t = transpose(m);
    for (const auto& row : t) std::cout << row[0] << ' ' << row[1] << '\n';
}
