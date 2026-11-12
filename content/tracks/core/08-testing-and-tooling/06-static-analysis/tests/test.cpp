#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "inventory.hpp"
#include "zth_test.hpp"

namespace {
const std::string kLong(48, 'x');  // longer than any small-string buffer: a copy allocates
std::vector<std::string> long_names() { return {kLong + "a", kLong + "bb", kLong + "c", kLong + "dd"}; }
}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "announce returns the line and stores the original message") {
    std::vector<std::string> log;
    ZTH_EXPECT_EQ(announce(log, "stock counted"), std::string("logged: stock counted"));
    ZTH_EXPECT_EQ(log.size(), std::size_t{1});
    ZTH_EXPECT_EQ(log[0], std::string("stock counted"));
}
ZTH_TEST("works", "total_length adds up the lengths") {
    ZTH_EXPECT_EQ(total_length({"bolt", "nut", "washer"}), std::size_t{13});
}
ZTH_TEST("works", "longest finds the longest name") {
    ZTH_EXPECT_EQ(longest({"bolt", "nut", "washer"}), std::string("washer"));
}
ZTH_TEST("works", "average gives a real number, not an integer division") {
    ZTH_EXPECT_NEAR(average(7, 2), 3.5, 1e-9);
    ZTH_EXPECT_NEAR(average(1, 3), 0.3333333333, 1e-6);
}
ZTH_TEST("works", "unit_price: full price below 10 units, 90% from 10 units on") {
    ZTH_EXPECT_NEAR(unit_price(2.0, 5), 2.0, 1e-9);
    ZTH_EXPECT_NEAR(unit_price(2.0, 10), 1.8, 1e-9);
    ZTH_EXPECT_NEAR(unit_price(2.0, 20), 1.8, 1e-9);
}
ZTH_TEST("works", "bucket_counts puts the values into four buckets") {
    ZTH_EXPECT_EQ(bucket_counts({1, 5, 12, 25, 99}), (std::vector<int>{2, 1, 1, 1}));
}
ZTH_TEST("works", "Inventory finds an item by name and returns a null pointer for an unknown one") {
    Inventory inv;
    inv.add({"bolt", 120, 0.25});
    inv.add({"nut", 80, 0.10});
    const Item* bolt = inv.find("bolt");
    ZTH_EXPECT_TRUE(bolt != nullptr);
    if (bolt) ZTH_EXPECT_EQ(bolt->quantity, 120);
    ZTH_EXPECT_TRUE(inv.find("washer") == nullptr);
    ZTH_EXPECT_EQ(inv.size(), std::size_t{2});
}
ZTH_TEST("works", "Stats computes the mean of the values it has seen") {
    Stats s;
    s.add(2.0);
    s.add(4.0);
    ZTH_EXPECT_NEAR(s.mean(), 3.0, 1e-9);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "announce with an empty message, and several messages in a row") {
    std::vector<std::string> log;
    ZTH_EXPECT_EQ(announce(log, ""), std::string("logged: "));
    ZTH_EXPECT_EQ(announce(log, "b"), std::string("logged: b"));
    ZTH_EXPECT_EQ(log.size(), std::size_t{2});
}
ZTH_TEST("robust", "total_length and longest of no names, and ties go to the first") {
    ZTH_EXPECT_EQ(total_length({}), std::size_t{0});
    ZTH_EXPECT_EQ(longest({}), std::string());
    ZTH_EXPECT_EQ(longest({"aa", "bb", "c"}), std::string("aa"));
}
ZTH_TEST("robust", "average of zero items is 0, and negative totals work") {
    ZTH_EXPECT_NEAR(average(5, 0), 0.0, 1e-12);
    ZTH_EXPECT_NEAR(average(-7, 2), -3.5, 1e-9);
}
ZTH_TEST("robust", "unit_price at the boundary: 9 units full price, 10 units discounted (off-by-one)") {
    ZTH_EXPECT_NEAR(unit_price(10.0, 9), 10.0, 1e-9);
    ZTH_EXPECT_NEAR(unit_price(10.0, 10), 9.0, 1e-9);
    ZTH_EXPECT_NEAR(unit_price(10.0, 0), 10.0, 1e-9);
}
ZTH_TEST("robust", "bucket_counts: no values, negatives, the edges of the buckets and a huge value") {
    ZTH_EXPECT_EQ(bucket_counts({}), (std::vector<int>{0, 0, 0, 0}));
    ZTH_EXPECT_EQ(bucket_counts({-5, -100}), (std::vector<int>{2, 0, 0, 0}));
    ZTH_EXPECT_EQ(bucket_counts({9, 10, 19, 20, 29, 30}), (std::vector<int>{1, 2, 2, 1}));
    ZTH_EXPECT_EQ(bucket_counts({1000000}), (std::vector<int>{0, 0, 0, 1}));
}
ZTH_TEST("robust", "an empty Inventory finds nothing, and the first of two equal names is returned") {
    Inventory inv;
    ZTH_EXPECT_TRUE(inv.find("x") == nullptr);
    inv.add({"x", 1, 1.0});
    inv.add({"x", 2, 2.0});
    const Item* x = inv.find("x");
    ZTH_EXPECT_TRUE(x != nullptr);
    if (x) ZTH_EXPECT_EQ(x->quantity, 1);
}
ZTH_TEST("robust", "a new Stats starts at zero, whatever was on the stack before") {
    ::zth::dirty_stack();
    Stats s;
    ZTH_EXPECT_EQ(s.count, 0);
    ZTH_EXPECT_NEAR(s.sum, 0.0, 1e-12);
    ZTH_EXPECT_NEAR(s.mean(), 0.0, 1e-12);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "total_length does not copy the vector of names") {
    const std::vector<std::string> names = long_names();
    ::zth::mem::Scope scope;
    const std::size_t n = total_length(names);
    ZTH_EXPECT_EQ(n, kLong.size() * 4 + 6);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "longest does not copy every name while it looks (only the result is a copy)") {
    const std::vector<std::string> names = long_names();
    ::zth::mem::Scope scope;
    const std::string best = longest(names);
    ZTH_EXPECT_EQ(best, kLong + "bb");
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3);  // `best = name` may allocate when it grows; a copy per name would allocate for each of the four names
}

ZTH_TEST("clean", "findings are fixed, not hidden: inventory.hpp has no NOLINT or cppcheck-suppress comment") {
    const std::string raw = ::zth::read_file("inventory.hpp");
    ZTH_EXPECT_TRUE(raw.find("NOLINT") == std::string::npos);
    ZTH_EXPECT_TRUE(raw.find("cppcheck-suppress") == std::string::npos);
}
