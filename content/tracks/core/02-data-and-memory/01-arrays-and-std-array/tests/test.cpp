#include <climits>
#include <vector>

#include "matrix.hpp"
#include "zth_test.hpp"

Matrix3x2 transpose(const Matrix2x3&);
long long sum_range(const int*, std::size_t);
int at_or(const std::array<int, 6>&, std::size_t, int);
std::size_t index_of(const std::array<int, 6>&, int);

namespace {
// The array sits directly in front of a sentinel, so reading one past the end (which compiles fine)
// returns 777 instead of being "probably zero".
struct Guarded {
    std::array<int, 6> a;
    volatile int sentinel = 777;
};
}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "transpose swaps rows and columns") {
    const Matrix2x3 m{{{1, 2, 3}, {4, 5, 6}}};
    const Matrix3x2 expected{{{1, 4}, {2, 5}, {3, 6}}};
    ZTH_EXPECT_TRUE(transpose(m) == expected);
}
ZTH_TEST("works", "sum_range adds up a C array") {
    const int raw[5] = {1, 2, 3, 4, 5};
    ZTH_EXPECT_EQ(sum_range(raw, 5), 15LL);
}
ZTH_TEST("works", "sum_range only adds the first `count` elements") {
    const int raw[5] = {1, 2, 3, 4, 100};
    ZTH_EXPECT_EQ(sum_range(raw, 4), 10LL);
}
ZTH_TEST("works", "at_or returns the element when the index is valid") {
    const std::array<int, 6> a{10, 11, 12, 13, 14, 15};
    ZTH_EXPECT_EQ(at_or(a, 0, -1), 10);
    ZTH_EXPECT_EQ(at_or(a, 5, -1), 15);
}
ZTH_TEST("works", "at_or returns the fallback when the index is too big") {
    Guarded g{{10, 11, 12, 13, 14, 15}};
    ZTH_EXPECT_EQ(at_or(g.a, 6, -1), -1);
}
ZTH_TEST("works", "index_of finds the first match") {
    const std::array<int, 6> a{4, 8, 8, 1, 9, 8};
    ZTH_EXPECT_EQ(index_of(a, 8), std::size_t{1});
    ZTH_EXPECT_EQ(index_of(a, 4), std::size_t{0});
}
ZTH_TEST("works", "index_of returns the size when the value is absent") {
    const std::array<int, 6> a{4, 8, 8, 1, 9, 8};
    ZTH_EXPECT_EQ(index_of(a, 5), std::size_t{6});
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "transpose with negative and zero values keeps every element in place") {
    const Matrix2x3 m{{{0, -1, 7}, {-8, 0, 9}}};
    const Matrix3x2 t = transpose(m);
    ZTH_EXPECT_EQ(t[0][0], 0);
    ZTH_EXPECT_EQ(t[0][1], -8);
    ZTH_EXPECT_EQ(t[1][0], -1);
    ZTH_EXPECT_EQ(t[1][1], 0);
    ZTH_EXPECT_EQ(t[2][0], 7);
    ZTH_EXPECT_EQ(t[2][1], 9);
}
ZTH_TEST("robust", "transpose does not modify its argument") {
    const Matrix2x3 m{{{1, 2, 3}, {4, 5, 6}}};
    Matrix2x3 copy = m;
    (void)transpose(copy);
    ZTH_EXPECT_TRUE(copy == m);
}
ZTH_TEST("robust", "sum_range of nothing is 0, even for a null pointer") {
    ZTH_EXPECT_EQ(sum_range(nullptr, 0), 0LL);
    const int raw[1] = {5};
    ZTH_EXPECT_EQ(sum_range(raw, 0), 0LL);
}
ZTH_TEST("robust", "sum_range does not overflow int") {
    const int raw[3] = {INT_MAX, INT_MAX, INT_MAX};
    ZTH_EXPECT_EQ(sum_range(raw, 3), 3LL * INT_MAX);
    const int neg[2] = {INT_MIN, INT_MIN};
    ZTH_EXPECT_EQ(sum_range(neg, 2), 2LL * INT_MIN);
}
ZTH_TEST("robust", "at_or handles huge indices, including -1 converted to size_t") {
    Guarded g{{1, 2, 3, 4, 5, 6}};
    ZTH_EXPECT_EQ(at_or(g.a, static_cast<std::size_t>(-1), 99), 99);
    ZTH_EXPECT_EQ(at_or(g.a, 1000000, 99), 99);
}
ZTH_TEST("robust", "at_or can return a fallback equal to a real element") {
    const std::array<int, 6> a{1, 2, 3, 4, 5, 6};
    ZTH_EXPECT_EQ(at_or(a, 2, 3), 3);
    ZTH_EXPECT_EQ(at_or(a, 6, 3), 3);
}
ZTH_TEST("robust", "index_of checks the last element too (off-by-one)") {
    const std::array<int, 6> a{1, 2, 3, 4, 5, 6};
    ZTH_EXPECT_EQ(index_of(a, 6), std::size_t{5});
}
