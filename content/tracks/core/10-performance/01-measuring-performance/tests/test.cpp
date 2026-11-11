#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "meter.hpp"
#include "report.hpp"
#include "zth_test.hpp"

namespace {

// The expected report, computed with plain ints (nothing counted).
std::string expected_report(const std::vector<Item>& items) {
    std::vector<std::pair<int, int>> unique;  // (id, qty) of the first item of each id, in order of first appearance
    std::map<int, bool> seen;
    for (const Item& i : items) {
        if (!seen[i.id]) {
            seen[i.id] = true;
            unique.emplace_back(i.id, i.qty);
        }
    }
    std::stable_sort(unique.begin(), unique.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::string out;
    for (const auto& u : unique) out += std::to_string(u.first) + ":" + std::to_string(u.second) + "\n";
    return out;
}

unsigned long long dedupe_compares(std::size_t n) {
    const std::vector<Item> items = make_items(n);
    meter::reset();
    const auto unique = dedupe(items);
    (void)unique;
    return meter::compares;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "hot_stage() names the stage that does the most work: look at the table that Run prints") {
    ZTH_EXPECT_EQ(std::string(hot_stage()), std::string("dedupe"));
}
ZTH_TEST("works", "the report is right for a small input (first item of each id, best qty first, ties in original order)") {
    const std::vector<Item> items = {{1, 5}, {2, 9}, {1, 99}, {3, 5}, {2, 1}, {4, 9}};
    ZTH_EXPECT_EQ(report(items), std::string("2:9\n4:9\n1:5\n3:5\n"));
}
ZTH_TEST("works", "the report matches the plain-int reference on 3000 generated items") {
    const std::vector<Item> items = make_items(3000);
    ZTH_EXPECT_EQ(report(items), expected_report(items));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "no items give an empty report") {
    ZTH_EXPECT_EQ(report(std::vector<Item>{}), std::string());
    ZTH_EXPECT_TRUE(dedupe(std::vector<Item>{}).empty());
}
ZTH_TEST("robust", "dedupe keeps the FIRST item of an id, not the last, and keeps the order of first appearance") {
    const std::vector<Item> items = {{7, 1}, {3, 2}, {7, 3}, {5, 4}, {3, 5}, {1, 6}};
    const auto unique = dedupe(items);
    ZTH_EXPECT_EQ(unique.size(), std::size_t{4});
    ZTH_EXPECT_EQ(unique[0].id, 7);
    ZTH_EXPECT_EQ(unique[0].qty, 1);
    ZTH_EXPECT_EQ(unique[1].id, 3);
    ZTH_EXPECT_EQ(unique[1].qty, 2);
    ZTH_EXPECT_EQ(unique[2].id, 5);
    ZTH_EXPECT_EQ(unique[3].id, 1);
}
ZTH_TEST("robust", "an id that appears many times is kept once (and one that appears once is kept)") {
    std::vector<Item> items(50, Item(4, 8));
    items.emplace_back(9, 1);
    const auto unique = dedupe(items);
    ZTH_EXPECT_EQ(unique.size(), std::size_t{2});
    ZTH_EXPECT_EQ(unique[1].id, 9);
}
ZTH_TEST("robust", "rank keeps the original order of items with the same qty (a stable sort)") {
    std::vector<Item> items;
    for (int i = 0; i < 300; ++i) items.emplace_back(i, i % 3);
    const auto ranked = rank(items);
    ZTH_EXPECT_EQ(ranked.size(), std::size_t{300});
    for (std::size_t i = 1; i < ranked.size(); ++i) {
        ZTH_EXPECT_TRUE(ranked[i - 1].qty >= ranked[i].qty);
        if (ranked[i - 1].qty == ranked[i].qty) ZTH_EXPECT_TRUE(ranked[i - 1].id < ranked[i].id);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "dedupe of 4000 items does at most 150000 comparisons (the starter does about 2.5 million)") {
    ZTH_EXPECT_TRUE(dedupe_compares(4000) <= 150000ULL);
}
ZTH_TEST("fast", "twice the items cost about twice the comparisons, not four times") {
    const auto a = dedupe_compares(2000);
    const auto b = dedupe_compares(4000);
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}
ZTH_TEST("fast", "the whole report copies each item only a few times (at most one per input item)") {
    const std::vector<Item> items = make_items(4000);
    meter::reset();
    const std::string r = report(items);
    ZTH_EXPECT_TRUE(!r.empty());
    ZTH_EXPECT_TRUE(meter::copies <= 4000ULL);
}
