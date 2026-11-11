#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "probe.hpp"
#include "slow.hpp"
#include "zth_test.hpp"

namespace {

std::vector<Tracked> numbers(int n) {
    std::vector<Tracked> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) v.emplace_back(i * 7 % 10007);  // distinct for n < 10007
    return v;
}
std::vector<std::string> words(int n, std::size_t length) {
    std::vector<std::string> w;
    for (int i = 0; i < n; ++i) {
        std::string s = "word-" + std::to_string(i) + "-";
        while (s.size() < length) s += 'x';
        w.push_back(s);
    }
    return w;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "reversed() reverses") {
    const std::vector<Tracked> v = {1, 2, 3, 4};
    const auto r = reversed(v);
    ZTH_EXPECT_EQ(r.size(), std::size_t{4});
    ZTH_EXPECT_EQ(r[0].v, 4);
    ZTH_EXPECT_EQ(r[1].v, 3);
    ZTH_EXPECT_EQ(r[3].v, 1);
}
ZTH_TEST("works", "weight_sum() adds up the values") {
    const std::vector<Tracked> v = {1, 2, 3, 4, 5};
    ZTH_EXPECT_EQ(weight_sum(v), 15LL);
}
ZTH_TEST("works", "join() puts the separator between the words") {
    ZTH_EXPECT_EQ(join({"a", "b", "c"}, ','), std::string("a,b,c"));
    ZTH_EXPECT_EQ(join({"one"}, '-'), std::string("one"));
}
ZTH_TEST("works", "has_duplicates() finds a repeated value") {
    ZTH_EXPECT_FALSE(has_duplicates(std::vector<Tracked>{1, 2, 3}));
    ZTH_EXPECT_TRUE(has_duplicates(std::vector<Tracked>{1, 2, 1}));
    ZTH_EXPECT_TRUE(has_duplicates(std::vector<Tracked>{5, 3, 9, 7, 3}));
}
ZTH_TEST("works", "lookup_count() finds a stored value, 0 for a missing key") {
    std::map<Tracked, int> m;
    m.emplace(Tracked(1), 10);
    m.emplace(Tracked(2), 20);
    ZTH_EXPECT_EQ(lookup_count(m, Tracked(2)), 20);
    ZTH_EXPECT_EQ(lookup_count(m, Tracked(3)), 0);
}
ZTH_TEST("works", "longest() returns the longest word") {
    ZTH_EXPECT_EQ(longest({"aa", "bbbb", "cc"}), std::string("bbbb"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "empty inputs: nothing to reverse, sum 0, empty join, no duplicates, empty longest") {
    ZTH_EXPECT_TRUE(reversed({}).empty());
    ZTH_EXPECT_EQ(weight_sum({}), 0LL);
    ZTH_EXPECT_EQ(join({}, ','), std::string());
    ZTH_EXPECT_FALSE(has_duplicates({}));
    ZTH_EXPECT_EQ(longest({}), std::string());
}
ZTH_TEST("robust", "one element, and a longer reverse in the right order") {
    const auto one = reversed(std::vector<Tracked>{9});
    ZTH_EXPECT_EQ(one.size(), std::size_t{1});
    ZTH_EXPECT_EQ(one[0].v, 9);
    const auto r = reversed(numbers(50));
    for (std::size_t i = 0; i < 50; ++i) ZTH_EXPECT_EQ(r[i].v, (49 - static_cast<int>(i)) * 7 % 10007);
}
ZTH_TEST("robust", "join: empty words are kept, and there is no separator before the first or after the last word") {
    ZTH_EXPECT_EQ(join({"", "a", ""}, ','), std::string(",a,"));
    ZTH_EXPECT_EQ(join({"", ""}, '/'), std::string("/"));
    ZTH_EXPECT_EQ(join({"x", "y"}, ' '), std::string("x y"));
}
ZTH_TEST("robust", "has_duplicates: a duplicate at the start, at the end, and equal neighbours; the input is not changed") {
    const std::vector<Tracked> a = {4, 1, 2, 3, 4};
    ZTH_EXPECT_TRUE(has_duplicates(a));
    ZTH_EXPECT_EQ(a[0].v, 4);
    ZTH_EXPECT_EQ(a[4].v, 4);
    ZTH_EXPECT_TRUE(has_duplicates(std::vector<Tracked>{8, 8}));
    ZTH_EXPECT_FALSE(has_duplicates(std::vector<Tracked>{8}));
    ZTH_EXPECT_FALSE(has_duplicates(std::vector<Tracked>{3, 2, 1, 0, -1}));
}
ZTH_TEST("robust", "lookup_count: a missing key is 0 and it adds nothing to the map") {
    std::map<Tracked, int> m;
    m.emplace(Tracked(5), 50);
    ZTH_EXPECT_EQ(lookup_count(m, Tracked(6)), 0);
    ZTH_EXPECT_EQ(m.size(), std::size_t{1});
    ZTH_EXPECT_EQ(lookup_count(m, Tracked(5)), 50);
    ZTH_EXPECT_EQ(lookup_count(std::map<Tracked, int>{}, Tracked(1)), 0);
}
ZTH_TEST("robust", "longest: the FIRST of the longest words wins, and a single word is returned") {
    ZTH_EXPECT_EQ(longest({"ab", "cd", "e"}), std::string("ab"));
    ZTH_EXPECT_EQ(longest({"a", "bc", "de", "f"}), std::string("bc"));
    ZTH_EXPECT_EQ(longest({"solo"}), std::string("solo"));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "1. reversed() of 2000 numbers copies each once and moves at most 2000 times (inserting at the front moved two million)") {
    const auto v = numbers(2000);
    probe::reset();
    const auto r = reversed(v);
    ZTH_EXPECT_EQ(r.size(), std::size_t{2000});
    ZTH_EXPECT_TRUE(probe::copies <= 2000);
    ZTH_EXPECT_TRUE(probe::moves <= 2000);
}
ZTH_TEST("fast", "2. weight_sum() of 2000 numbers makes no copy at all (it passed the whole vector by value for every element)") {
    const auto v = numbers(2000);
    probe::reset();
    const long long s = weight_sum(v);
    ZTH_EXPECT_TRUE(s > 0);
    ZTH_EXPECT_EQ(probe::copies, 0ULL);
}
ZTH_TEST("fast", "3. join() of 1000 words needs at most 3 heap allocations (+ builds a temporary string every time)") {
    const auto w = words(1000, 24);
    zth::mem::Scope scope;
    const std::string s = join(w, ',');
    ZTH_EXPECT_EQ(s.size(), std::size_t{1000 * 24 + 999});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3);
}
ZTH_TEST("fast", "4. has_duplicates() of 2000 different numbers does at most 60000 comparisons (two million with two nested loops)") {
    const auto v = numbers(2000);
    probe::reset();
    ZTH_EXPECT_FALSE(has_duplicates(v));
    ZTH_EXPECT_TRUE(probe::compares <= 60000);
    ZTH_EXPECT_TRUE(probe::copies <= 2100);
}
ZTH_TEST("fast", "5. lookup_count() searches the map ONCE: 100 lookups in a map of 1024 do at most 1600 comparisons") {
    std::map<Tracked, int> m;
    for (int i = 0; i < 1024; ++i) m.emplace(Tracked(i), i);
    probe::reset();
    long long sum = 0;
    for (int i = 0; i < 100; ++i) sum += lookup_count(m, Tracked(i * 9));
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_TRUE(probe::compares <= 1600);
}
ZTH_TEST("fast", "6. longest() of 1000 words copies at most the result (a loop variable that is a copy allocates for every word)") {
    std::vector<std::string> w;  // every word is one character longer than the one before
    for (int i = 0; i < 1000; ++i) w.emplace_back(static_cast<std::size_t>(20 + i), 'x');
    zth::mem::Scope scope;
    const std::string s = longest(w);
    ZTH_EXPECT_EQ(s.size(), std::size_t{1019});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "slow.hpp has no raw new/delete/malloc/free") {
    ZTH_EXPECT_NO_RAW_MEMORY("slow.hpp");
}
ZTH_TEST("clean", "slow.hpp does not use std::endl or `using namespace`") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("slow.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "endl").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "using").empty());
}
