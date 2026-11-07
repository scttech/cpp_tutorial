#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "bugs.hpp"
#include "zth_test.hpp"

using V = std::vector<int>;

namespace {
struct Canary {
    int data[8];
    volatile int canary = 0x7E57;
};
const std::string kLong = "a label that is far too long for the small string buffer";
}  // namespace

// The guard value after an array must still be 0x7E57 (32343); if not, something wrote past the end.
#define EXPECT_CANARY(c)                                                                                        do {                                                                                                            const int zth_seen = (c).canary;                                                                            if (zth_seen != 0x7E57) {                                                                                       zth::fail("something wrote past the end of the array: the guard value after it changed from "                        "32343 to " + std::to_string(zth_seen));                                                          return;                                                                                                 }                                                                                                       } while (0)

// ---- Works: one test per bug ---------------------------------------------------------------------
ZTH_TEST("works", "bug 1: fill_squares writes exactly n squares and nothing after them") {
    Canary c{{-1, -1, -1, -1, -1, -1, -1, -1}};
    fill_squares(c.data, 8);
    EXPECT_CANARY(c);  // the memory right after the array must be untouched
    ZTH_EXPECT_EQ(c.data[7], 49);
    ZTH_EXPECT_EQ(c.data[3], 9);
}
ZTH_TEST("works", "bug 2: a Label shouts the text it was made with") {
    Label l = make_label(kLong.c_str());
    ZTH_EXPECT_EQ(l.shout(), kLong + " (label)!");
}
ZTH_TEST("works", "bug 3: take_and_release returns the value that was stored") {
    int* p = new int(42);
    ZTH_EXPECT_EQ(take_and_release(p), 42);
}
ZTH_TEST("works", "bug 4: first_after_growth returns v[0] after the vector grew") {
    V v;
    v.reserve(4);  // full after four elements: the next push_back must reallocate
    for (int x : {10, 20, 30, 40}) v.push_back(x);
    ZTH_EXPECT_EQ(first_after_growth(v, 50), 10);
    ZTH_EXPECT_EQ(v, (V{10, 20, 30, 40, 50}));
}
ZTH_TEST("works", "bug 5: count_positive counts the positive elements") {
    const V v{1, -2, 3, 0, 5};
    zth::dirty_stack();  // fills the stack with 0x5A5A5A5A so that an uninitialized local is not 'luckily' zero
    ZTH_EXPECT_EQ(count_positive(v), 3);
}
ZTH_TEST("works", "bug 6: an Entry keeps its full name") {
    Entry e = make_entry(kLong, "Lovelace", 7);
    ZTH_EXPECT_EQ(std::string(e.name), kLong + " Lovelace");
    ZTH_EXPECT_EQ(e.score, 7);
}

// ---- Robust: the fixes must be real, not lucky ----------------------------------------------------
ZTH_TEST("robust", "bug 1: n = 0 writes nothing; n = 1 writes one element; n = 1000 stays in bounds") {
    Canary c{{5, 5, 5, 5, 5, 5, 5, 5}};
    fill_squares(c.data, 0);
    ZTH_EXPECT_EQ(c.data[0], 5);
    EXPECT_CANARY(c);
    fill_squares(c.data, 1);
    ZTH_EXPECT_EQ(c.data[0], 0);
    ZTH_EXPECT_EQ(c.data[1], 5);
    std::vector<int> big(1001, -1);
    fill_squares(big.data(), 1000);
    ZTH_EXPECT_EQ(big[999], 999 * 999);
    ZTH_EXPECT_EQ(big[1000], -1);
}
ZTH_TEST("robust", "bug 2: a Label keeps its own copy of the text (the original may change or disappear)") {
    std::string src = kLong;
    Label l(src);
    src = "something else entirely";
    ZTH_EXPECT_EQ(l.shout(), kLong + "!");
    Label copy = l;
    ZTH_EXPECT_EQ(copy.shout(), kLong + "!");
}
ZTH_TEST("robust", "bug 3: take_and_release frees the block exactly once (no leak, no double free)") {
    zth::mem::Scope m;
    int* p = new int(7);
    int v = take_and_release(p);
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
    ZTH_EXPECT_EQ(v, 7);
}
ZTH_TEST("robust", "bug 4: first_after_growth is right whether or not the vector had to reallocate") {
    V roomy;
    roomy.reserve(100);
    roomy.push_back(5);
    ZTH_EXPECT_EQ(first_after_growth(roomy, 6), 5);
    ZTH_EXPECT_EQ(roomy, (V{5, 6}));
    V one;
    one.push_back(9);  // capacity 1: growing reallocates
    ZTH_EXPECT_EQ(first_after_growth(one, 8), 9);
    ZTH_EXPECT_EQ(one, (V{9, 8}));
}
ZTH_TEST("robust", "bug 5: count_positive of an empty vector is 0, of all-negative is 0, of all-positive is its size") {
    zth::dirty_stack();
    ZTH_EXPECT_EQ(count_positive(V{}), 0);
    zth::dirty_stack();
    ZTH_EXPECT_EQ(count_positive(V{-1, -2, -3}), 0);
    zth::dirty_stack();
    ZTH_EXPECT_EQ(count_positive(V(1000, 4)), 1000);
}
ZTH_TEST("robust", "bug 6: an Entry stays valid after the strings it was built from are gone") {
    Entry e = [] {
        std::string a = kLong, b = "Byron";
        return make_entry(a, b, 1);
    }();
    std::vector<std::string> churn;  // reuse (and poison) memory, in case the fix only works by luck
    for (int i = 0; i < 50; ++i) churn.push_back(std::string(64, 'x'));
    ZTH_EXPECT_EQ(std::string(e.name), kLong + " Byron");
}
ZTH_TEST("robust", "bug 6: short names work too, and entries can be copied") {
    Entry a = make_entry("Al", "Bo", 3);
    Entry b = a;
    ZTH_EXPECT_EQ(std::string(b.name), "Al Bo");
    ZTH_EXPECT_EQ(b.score, 3);
}
