#include <climits>
#include <vector>

#include "zth_test.hpp"

void swap_values(int&, int&);
void rotate3(int&, int&, int&);
const int& larger(const int&, const int&);
int& largest_in(std::vector<int>&);
void clamp_all(std::vector<int>&, int, int);
int& element_or(std::vector<int>&, std::size_t, int&);

using V = std::vector<int>;

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "swap_values exchanges two variables") {
    int a = 1, b = 2;
    swap_values(a, b);
    ZTH_EXPECT_EQ(a, 2);
    ZTH_EXPECT_EQ(b, 1);
}
ZTH_TEST("works", "rotate3: a gets b, b gets c, c gets the old a") {
    int a = 1, b = 2, c = 3;
    rotate3(a, b, c);
    ZTH_EXPECT_EQ(a, 2);
    ZTH_EXPECT_EQ(b, 3);
    ZTH_EXPECT_EQ(c, 1);
}
ZTH_TEST("works", "larger returns the larger value") {
    int a = 3, b = 8;
    ZTH_EXPECT_EQ(larger(a, b), 8);
    ZTH_EXPECT_EQ(larger(b, a), 8);
}
ZTH_TEST("works", "larger returns a reference to the argument itself, not a copy") {
    int a = 3, b = 8;
    ZTH_EXPECT_TRUE(&larger(a, b) == &b);
    ZTH_EXPECT_TRUE(&larger(b, a) == &b);
}
ZTH_TEST("works", "largest_in lets the caller change the largest element") {
    V v{4, 9, 2};
    largest_in(v) = 0;
    ZTH_EXPECT_EQ(v, (V{4, 0, 2}));
}
ZTH_TEST("works", "clamp_all limits every element") {
    V v{-5, 0, 3, 10, 7};
    clamp_all(v, 0, 5);
    ZTH_EXPECT_EQ(v, (V{0, 0, 3, 5, 5}));
}
ZTH_TEST("works", "element_or gives access to the element when the index is valid") {
    V v{10, 20, 30};
    int fb = -1;
    element_or(v, 1, fb) = 99;
    ZTH_EXPECT_EQ(v, (V{10, 99, 30}));
    ZTH_EXPECT_EQ(fb, -1);
}
ZTH_TEST("works", "element_or gives access to the fallback when it is not") {
    V v{10, 20, 30};
    int fb = -1;
    element_or(v, 3, fb) = 99;
    ZTH_EXPECT_EQ(fb, 99);
    ZTH_EXPECT_EQ(v, (V{10, 20, 30}));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "swap_values of a variable with itself leaves it unchanged") {
    int x = 5;
    swap_values(x, x);
    ZTH_EXPECT_EQ(x, 5);
}
ZTH_TEST("robust", "swap_values with extreme and negative values") {
    int a = INT_MIN, b = INT_MAX;
    swap_values(a, b);
    ZTH_EXPECT_EQ(a, INT_MAX);
    ZTH_EXPECT_EQ(b, INT_MIN);
}
ZTH_TEST("robust", "rotate3 three times gives back the original") {
    int a = 7, b = -8, c = 9;
    rotate3(a, b, c);
    rotate3(a, b, c);
    rotate3(a, b, c);
    ZTH_EXPECT_EQ(a, 7);
    ZTH_EXPECT_EQ(b, -8);
    ZTH_EXPECT_EQ(c, 9);
}
ZTH_TEST("robust", "larger: equal arguments return the first one; negatives work") {
    int a = 4, b = 4;
    ZTH_EXPECT_TRUE(&larger(a, b) == &a);
    int n = -9, m = -3;
    ZTH_EXPECT_TRUE(&larger(n, m) == &m);
}
ZTH_TEST("robust", "largest_in: ties return the FIRST maximum; a single element works") {
    V v{1, 7, 3, 7, 7};
    ZTH_EXPECT_TRUE(&largest_in(v) == &v[1]);
    V one{-4};
    ZTH_EXPECT_TRUE(&largest_in(one) == &one[0]);
}
ZTH_TEST("robust", "largest_in returns a reference into the vector (writes are visible, address matches)") {
    V v{3, 1, 4, 1, 5, 9, 2, 6};
    int& m = largest_in(v);
    ZTH_EXPECT_TRUE(&m == &v[5]);
    m += 1;
    ZTH_EXPECT_EQ(v[5], 10);
}
ZTH_TEST("robust", "clamp_all: empty vector, lo == hi, values already inside, extremes") {
    V empty;
    clamp_all(empty, 0, 1);
    ZTH_EXPECT_EQ(empty, V{});
    V same{1, 2, 3};
    clamp_all(same, 2, 2);
    ZTH_EXPECT_EQ(same, (V{2, 2, 2}));
    V inside{1, 2, 3};
    clamp_all(inside, 0, 10);
    ZTH_EXPECT_EQ(inside, (V{1, 2, 3}));
    V ext{INT_MIN, INT_MAX};
    clamp_all(ext, -10, 10);
    ZTH_EXPECT_EQ(ext, (V{-10, 10}));
}
ZTH_TEST("robust", "element_or: the result really is the element or the fallback object") {
    V v{1, 2, 3};
    int fb = 0;
    ZTH_EXPECT_TRUE(&element_or(v, 2, fb) == &v[2]);
    ZTH_EXPECT_TRUE(&element_or(v, 3, fb) == &fb);
    ZTH_EXPECT_TRUE(&element_or(v, static_cast<std::size_t>(-1), fb) == &fb);
}
ZTH_TEST("robust", "element_or on an empty vector returns the fallback") {
    V v;
    int fb = 5;
    ZTH_EXPECT_TRUE(&element_or(v, 0, fb) == &fb);
}
