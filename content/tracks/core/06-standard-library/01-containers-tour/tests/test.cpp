#include <algorithm>
#include <cstddef>
#include <iterator>
#include <list>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "containers.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using V = std::vector<int>;
using Counts = std::map<std::string, int>;

// an element that counts how often it is compared
struct Counted {
    int v = 0;
    static inline long compares = 0;
    Counted() = default;
    Counted(int x) : v(x) {}  // NOLINT
    bool operator<(const Counted& o) const {
        ++compares;
        return v < o.v;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "word_counts counts each word") {
    const Counts c = word_counts({"b", "a", "b", "c", "b"});
    ZTH_EXPECT_EQ(c.size(), std::size_t{3});
    ZTH_EXPECT_EQ(c.at("a"), 1);
    ZTH_EXPECT_EQ(c.at("b"), 3);
    ZTH_EXPECT_EQ(c.at("c"), 1);
}
ZTH_TEST("works", "word_counts: the keys are sorted (that is what a std::map gives for free)") {
    const Counts c = word_counts({"pear", "apple", "fig", "apple"});
    std::vector<S> keys;
    for (const auto& kv : c) keys.push_back(kv.first);
    ZTH_EXPECT_TRUE((keys == std::vector<S>{"apple", "fig", "pear"}));
}
ZTH_TEST("works", "count_of returns the count, and 0 for a word that is not there") {
    Counts c = word_counts({"x", "y", "x"});
    ZTH_EXPECT_EQ(count_of(c, "x"), 2);
    ZTH_EXPECT_EQ(count_of(c, "y"), 1);
    ZTH_EXPECT_EQ(count_of(c, "nope"), 0);
}
ZTH_TEST("works", "count_of is a LOOKUP: it must not add the missing word (operator[] would)") {
    Counts c = word_counts({"x"});
    count_of(c, "missing");
    count_of(c, "another");
    ZTH_EXPECT_EQ(c.size(), std::size_t{1});
    ZTH_EXPECT_TRUE(c.find("missing") == c.end());
}
ZTH_TEST("works", "dedupe keeps the first occurrence of each value, in order") {
    ZTH_EXPECT_TRUE((dedupe({3, 1, 3, 2, 1}) == V{3, 1, 2}));
    ZTH_EXPECT_TRUE((dedupe({5, 5, 5}) == V{5}));
    ZTH_EXPECT_TRUE((dedupe({1, 2, 3}) == V{1, 2, 3}));
}
ZTH_TEST("works", "process_queue: take the front job, record it, and queue half of it at the back") {
    ZTH_EXPECT_TRUE((process_queue({5, 3}) == V{5, 3, 2, 1, 1}));
    ZTH_EXPECT_TRUE((process_queue({1}) == V{1}));
    ZTH_EXPECT_TRUE((process_queue({8}) == V{8, 4, 2, 1}));
}
ZTH_TEST("works", "move_to_front moves an element to the front") {
    std::list<int> l{1, 2, 3, 4};
    move_to_front(l, std::next(l.begin(), 2));
    ZTH_EXPECT_TRUE((std::vector<int>(l.begin(), l.end()) == V{3, 1, 2, 4}));
    move_to_front(l, std::prev(l.end()));
    ZTH_EXPECT_TRUE((std::vector<int>(l.begin(), l.end()) == V{4, 3, 1, 2}));
}
ZTH_TEST("works", "k_smallest: the k smallest values, ascending") {
    ZTH_EXPECT_TRUE((k_smallest(V{9, 4, 7, 1, 8, 2}, 3) == V{1, 2, 4}));
    ZTH_EXPECT_TRUE((k_smallest(V{5, 4, 3, 2, 1}, 1) == V{1}));
    ZTH_EXPECT_TRUE((k_smallest(V{3, 1, 2}, 3) == V{1, 2, 3}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "word_counts of nothing is an empty map; an empty string is a word like any other") {
    ZTH_EXPECT_TRUE(word_counts({}).empty());
    const Counts c = word_counts({"", "", "a"});
    ZTH_EXPECT_EQ(c.at(""), 2);
}
ZTH_TEST("robust", "word_counts is case sensitive (Apple and apple are different keys)") {
    const Counts c = word_counts({"Apple", "apple", "Apple"});
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    ZTH_EXPECT_EQ(c.at("Apple"), 2);
}
ZTH_TEST("robust", "count_of on an empty map, and for a key that is only a prefix of another") {
    Counts empty;
    ZTH_EXPECT_EQ(count_of(empty, "a"), 0);
    ZTH_EXPECT_TRUE(empty.empty());
    Counts c = word_counts({"cart"});
    ZTH_EXPECT_EQ(count_of(c, "car"), 0);
    ZTH_EXPECT_EQ(c.size(), std::size_t{1});
}
ZTH_TEST("robust", "dedupe: empty input, negative numbers and values that look alike") {
    ZTH_EXPECT_TRUE(dedupe({}).empty());
    ZTH_EXPECT_TRUE((dedupe({-1, 1, -1, 0, 0}) == V{-1, 1, 0}));
    ZTH_EXPECT_TRUE((dedupe({10, 1, 10, 100, 1}) == V{10, 1, 100}));
}
ZTH_TEST("robust", "dedupe does not modify or sort its input") {
    const V in{3, 1, 3, 2};
    const V out = dedupe(in);
    ZTH_EXPECT_TRUE((in == V{3, 1, 3, 2}));
    ZTH_EXPECT_TRUE((out == V{3, 1, 2}));
}
ZTH_TEST("robust", "process_queue of nothing, and of jobs that need no follow-up") {
    ZTH_EXPECT_TRUE(process_queue({}).empty());
    ZTH_EXPECT_TRUE((process_queue({1, 1, 1}) == V{1, 1, 1}));
    ZTH_EXPECT_TRUE((process_queue({0}) == V{0}));
}
ZTH_TEST("robust", "process_queue interleaves the follow-ups behind the original jobs (FIFO)") {
    ZTH_EXPECT_TRUE((process_queue({4, 6}) == V{4, 6, 2, 3, 1, 1}));
}
ZTH_TEST("robust", "move_to_front of the first element changes nothing; of a one-element list too") {
    std::list<int> l{1, 2, 3};
    move_to_front(l, l.begin());
    ZTH_EXPECT_TRUE((std::vector<int>(l.begin(), l.end()) == V{1, 2, 3}));
    std::list<int> one{7};
    move_to_front(one, one.begin());
    ZTH_EXPECT_EQ(one.size(), std::size_t{1});
    ZTH_EXPECT_EQ(one.front(), 7);
}
ZTH_TEST("robust", "move_to_front keeps the iterator valid: it still refers to the same element") {
    std::list<S> l{"a", "b", "c", "d"};
    auto it = std::next(l.begin(), 3);
    const S* address = &*it;
    move_to_front(l, it);
    ZTH_EXPECT_TRUE(it == l.begin());
    ZTH_EXPECT_TRUE(&*it == address);
    ZTH_EXPECT_EQ(*it, S("d"));
}
ZTH_TEST("robust", "k_smallest with k == 0, k larger than the input, an empty input and duplicates") {
    ZTH_EXPECT_TRUE(k_smallest(V{3, 1, 2}, 0).empty());
    ZTH_EXPECT_TRUE((k_smallest(V{3, 1, 2}, 10) == V{1, 2, 3}));
    ZTH_EXPECT_TRUE(k_smallest(V{}, 4).empty());
    ZTH_EXPECT_TRUE((k_smallest(V{2, 2, 1, 2, 1}, 3) == V{1, 1, 2}));
}
ZTH_TEST("robust", "k_smallest does not change its input and works for strings") {
    const V in{5, 3, 9};
    k_smallest(in, 2);
    ZTH_EXPECT_TRUE((in == V{5, 3, 9}));
    ZTH_EXPECT_TRUE((k_smallest(std::vector<S>{"pear", "fig", "apple", "kiwi"}, 2) == std::vector<S>{"apple", "fig"}));
}
ZTH_TEST("robust", "k_smallest agrees with sorting, for random data and several k") {
    zth::Lcg rng(5);
    for (int round = 0; round < 40; ++round) {
        V v;
        const int n = rng.below(60);
        for (int i = 0; i < n; ++i) v.push_back(rng.below(30));
        const std::size_t k = static_cast<std::size_t>(rng.below(20));
        V expected = v;
        std::sort(expected.begin(), expected.end());
        if (expected.size() > k) expected.resize(k);
        if (k_smallest(v, k) != expected) {
            ::zth::fail("k_smallest differs from the first k of the sorted input (round " + std::to_string(round) + ")");
            return;
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "dedupe handles 300000 values in well under 2 seconds (a set, not a search through the result)") {
    V in;
    in.reserve(300000);
    for (int i = 0; i < 300000; ++i) in.push_back((i * 7) % 250000);  // many distinct values, some repeats
    const V out = dedupe(in);
    ZTH_EXPECT_EQ(out.size(), std::size_t{250000});
    ZTH_EXPECT_EQ(out.front(), 0);
}
ZTH_TEST("fast", "process_queue handles 100000 jobs (a deque, not a vector erased at the front)") {
    V jobs;
    jobs.reserve(100000);
    for (int i = 0; i < 100000; ++i) jobs.push_back(1 + (i * 37) % 1000);
    const V order = process_queue(jobs);
    ZTH_EXPECT_TRUE(order.size() > 700000);
    ZTH_EXPECT_EQ(order.front(), 1);
    ZTH_EXPECT_EQ(order[1], 38);
}
ZTH_TEST("fast", "move_to_front copies and moves NO element (it relinks the node)") {
    std::list<zth::Tracked> l;
    for (int i = 1; i <= 5; ++i) l.emplace_back(i);
    auto it = std::next(l.begin(), 4);
    zth::Tracked::reset();
    move_to_front(l, it);
    ZTH_EXPECT_EQ(zth::Tracked::copies + zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns + zth::Tracked::move_assigns, 0);
    ZTH_EXPECT_EQ(zth::Tracked::constructed(), 0);
    ZTH_EXPECT_EQ(l.front().value, 5);
    ZTH_EXPECT_EQ(l.back().value, 4);
}
ZTH_TEST("fast", "k_smallest of 10000 values for k = 10 does far fewer comparisons than sorting them all") {
    zth::Lcg rng(99);
    std::vector<Counted> values;
    for (int i = 0; i < 10000; ++i) values.push_back(Counted(rng.below(1000000)));
    Counted::compares = 0;
    const std::vector<Counted> best = k_smallest(values, 10);
    const long compares = Counted::compares;
    ZTH_EXPECT_EQ(best.size(), std::size_t{10});
    for (std::size_t i = 1; i < best.size(); ++i) ZTH_EXPECT_TRUE(best[i - 1].v <= best[i].v);
    if (compares > 50000) {
        ::zth::fail("k_smallest made " + std::to_string(compares) + " comparisons: sorting all 10000 values takes about 130000. Keep a heap of the k best instead.");
    }
}
