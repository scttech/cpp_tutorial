#pragma once
#include <array>
#include <cstddef>

// A matrix with 2 rows and 3 columns, and its transpose (3 rows, 2 columns).
// std::array<T, N> is a real value type: it can be copied, returned and compared.
using Matrix2x3 = std::array<std::array<int, 3>, 2>;
using Matrix3x2 = std::array<std::array<int, 2>, 3>;
