#include <climits>
#include <numeric>
#include <vector>

#include "zth_test.hpp"

std::vector<int> rotate_and_dedupe(const std::vector<int>&, int);

using V = std::vector<int>;

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "rotates left: {1,2,3,4,5} by 2 is {3,4,5,1,2}") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3, 4, 5}, 2), (V{3, 4, 5, 1, 2}));
}
ZTH_TEST("works", "k = 0 keeps the order") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({4, 5, 6}, 0), (V{4, 5, 6}));
}
ZTH_TEST("works", "removes later duplicates and keeps the original order") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({3, 1, 3, 2, 1}, 0), (V{3, 1, 2}));
}
ZTH_TEST("works", "rotation happens first: {1,2,1,3} by 1 is {2,1,3}") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 1, 3}, 1), (V{2, 1, 3}));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "empty input gives empty output for any k") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({}, 0), V{});
    ZTH_EXPECT_EQ(rotate_and_dedupe({}, 7), V{});
    ZTH_EXPECT_EQ(rotate_and_dedupe({}, -3), V{});
}
ZTH_TEST("robust", "k larger than the size wraps around: {1,2,3} by 5 is {3,1,2}") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, 5), (V{3, 1, 2}));
}
ZTH_TEST("robust", "k equal to a multiple of the size changes nothing") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, 3), (V{1, 2, 3}));
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, 300), (V{1, 2, 3}));
}
ZTH_TEST("robust", "negative k rotates right: {1,2,3,4,5} by -1 is {5,1,2,3,4}") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3, 4, 5}, -1), (V{5, 1, 2, 3, 4}));
}
ZTH_TEST("robust", "negative k larger than the size wraps: {1,2,3} by -4 is {3,1,2}") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, -4), (V{3, 1, 2}));
}
ZTH_TEST("robust", "extreme k values (INT_MAX, INT_MIN) do not overflow") {
    // INT_MAX = 2147483647 = 3 * 715827882 + 1 -> shift 1;  INT_MIN = -2147483648 = -3 * 715827883 + 1 -> shift 1
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, INT_MAX), (V{2, 3, 1}));
    ZTH_EXPECT_EQ(rotate_and_dedupe({1, 2, 3}, INT_MIN), (V{2, 3, 1}));
}
ZTH_TEST("robust", "single element and all-equal inputs") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({9}, 4), V{9});
    ZTH_EXPECT_EQ(rotate_and_dedupe({7, 7, 7, 7}, 2), V{7});
}
ZTH_TEST("robust", "negative and zero values are ordinary values") {
    ZTH_EXPECT_EQ(rotate_and_dedupe({0, -1, 0, -1, 2}, 1), (V{-1, 0, 2}));
}
ZTH_TEST("robust", "the input vector is not modified") {
    const V in{5, 4, 5, 3};
    const V copy = in;
    (void)rotate_and_dedupe(in, 2);
    ZTH_EXPECT_EQ(in, copy);
}
ZTH_TEST("robust", "stays fast for 200 000 distinct values") {
    V big(200000);
    std::iota(big.begin(), big.end(), 0);
    V out = rotate_and_dedupe(big, 12345);
    ZTH_EXPECT_EQ(out.size(), std::size_t{200000});
    ZTH_EXPECT_EQ(out.front(), 12345);
    ZTH_EXPECT_EQ(out.back(), 12344);
}
