#define ZTH_TRACK_ALLOCATIONS
#include <climits>
#include <vector>

#include "rle.hpp"
#include "zth_test.hpp"

std::vector<Run> encode(const std::vector<int>&);
std::vector<int> decode(const std::vector<Run>&);
std::vector<long long> squares(int);
std::vector<int> make_buffer(std::size_t);
void remove_evens(std::vector<int>&);

using Runs = std::vector<Run>;
using V = std::vector<int>;

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "encode merges adjacent equal values") {
    ZTH_EXPECT_EQ(encode({5, 5, 5, 2, 2, 9}), (Runs{{5, 3}, {2, 2}, {9, 1}}));
}
ZTH_TEST("works", "encode does not merge values that are not adjacent") {
    ZTH_EXPECT_EQ(encode({1, 2, 1}), (Runs{{1, 1}, {2, 1}, {1, 1}}));
}
ZTH_TEST("works", "decode expands each run") {
    ZTH_EXPECT_EQ(decode({{5, 3}, {2, 2}, {9, 1}}), (V{5, 5, 5, 2, 2, 9}));
}
ZTH_TEST("works", "squares(5) is 0 1 4 9 16") {
    ZTH_EXPECT_EQ(squares(5), (std::vector<long long>{0, 1, 4, 9, 16}));
}
ZTH_TEST("works", "make_buffer returns an empty vector with room for n elements") {
    V b = make_buffer(50);
    ZTH_EXPECT_EQ(b.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(b.capacity() >= 50);
}
ZTH_TEST("works", "remove_evens keeps the odd numbers in order") {
    V v{1, 2, 3, 4, 5, 6, 7};
    remove_evens(v);
    ZTH_EXPECT_EQ(v, (V{1, 3, 5, 7}));
}
ZTH_TEST("works", "remove_evens handles consecutive even numbers (erasing in a loop skips the second one)") {
    V v{2, 4, 6, 1, 8, 10, 3};
    remove_evens(v);
    ZTH_EXPECT_EQ(v, (V{1, 3}));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "encode and decode of an empty input are empty") {
    ZTH_EXPECT_EQ(encode({}), Runs{});
    ZTH_EXPECT_EQ(decode({}), V{});
}
ZTH_TEST("robust", "encode of a single value, and of one long run") {
    ZTH_EXPECT_EQ(encode({7}), (Runs{{7, 1}}));
    V many(100000, 4);
    ZTH_EXPECT_EQ(encode(many), (Runs{{4, 100000}}));
}
ZTH_TEST("robust", "encode handles negative values and alternating values") {
    ZTH_EXPECT_EQ(encode({-1, -1, 0, 0, -1, 0}), (Runs{{-1, 2}, {0, 2}, {-1, 1}, {0, 1}}));
}
ZTH_TEST("robust", "decode of encode gives back the original") {
    const V original{3, 3, 3, 1, 1, 2, 3, 3, 3, 3, 0, -5, -5};
    ZTH_EXPECT_EQ(decode(encode(original)), original);
}
ZTH_TEST("robust", "decode ignores runs whose count is zero or negative") {
    ZTH_EXPECT_EQ(decode({{1, 2}, {9, 0}, {8, -3}, {2, 1}}), (V{1, 1, 2}));
    ZTH_EXPECT_EQ(decode({{8, -3}}), V{});  // a lone negative count: converting it to size_t gives a gigantic number
}
ZTH_TEST("robust", "squares of n <= 0 is empty") {
    ZTH_EXPECT_EQ(squares(0), std::vector<long long>{});
    ZTH_EXPECT_EQ(squares(-5), std::vector<long long>{});
}
ZTH_TEST("robust", "squares does not overflow: 99999 * 99999 = 9999800001") {
    auto s = squares(100000);
    ZTH_EXPECT_EQ(s.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(s.back(), 9999800001LL);
    ZTH_EXPECT_EQ(s[46341], 46341LL * 46341LL);  // first value that no longer fits in an int
}
ZTH_TEST("robust", "make_buffer(0) is empty") {
    ZTH_EXPECT_EQ(make_buffer(0).size(), std::size_t{0});
}
ZTH_TEST("robust", "remove_evens: all even, all odd, empty, zero and negatives") {
    V a{2, 4, 6};
    remove_evens(a);
    ZTH_EXPECT_EQ(a, V{});
    V b{1, 3, 5};
    remove_evens(b);
    ZTH_EXPECT_EQ(b, (V{1, 3, 5}));
    V c;
    remove_evens(c);
    ZTH_EXPECT_EQ(c, V{});
    V d{0, -1, -2, -3, -4, 5};
    remove_evens(d);
    ZTH_EXPECT_EQ(d, (V{-1, -3, 5}));
}

// ---- Fast: heap allocations are counted --------------------------------------------------------
ZTH_TEST("fast", "squares(1000) uses a single allocation (reserve)") {
    zth::mem::Scope m;
    auto s = squares(1000);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
    ZTH_EXPECT_EQ(s.size(), std::size_t{1000});
}
ZTH_TEST("fast", "decode uses a single allocation (work out the size first)") {
    Runs runs;
    for (int i = 0; i < 200; ++i) runs.push_back({i, 50});
    zth::mem::Scope m;
    V out = decode(runs);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
    ZTH_EXPECT_EQ(out.size(), std::size_t{10000});
}
ZTH_TEST("fast", "remove_evens works in place: no allocation at all") {
    V v;
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    zth::mem::Scope m;
    remove_evens(v);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 0);
    ZTH_EXPECT_EQ(v.size(), std::size_t{500});
}
