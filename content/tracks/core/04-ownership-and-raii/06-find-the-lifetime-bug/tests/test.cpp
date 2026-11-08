#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "bugs.hpp"
#include "zth_test.hpp"

namespace {

using V = std::vector<int>;
using Words = std::vector<std::string>;

// the expected sentence for describe(n)
std::string sentence(int n) {
    return "number " + std::to_string(n) + (n % 2 != 0 ? " is odd" : " is even") + " (and this sentence is long enough to need the heap)";
}
std::string label(int id) { return "item number " + std::to_string(id) + " in the long warehouse"; }

// A function that does a lot of work on the stack and the heap, to overwrite whatever a dangling pointer pointed at.
int churn() {
    int total = 0;
    for (int i = 0; i < 50; ++i) {
        std::string junk(100 + i, static_cast<char>('a' + i % 26));
        std::vector<int> v(64, i);
        total += static_cast<int>(junk.size() + v.size());
    }
    return total;
}

}  // namespace

// ---- Works: one test per bug ----------------------------------------------------------------------------
ZTH_TEST("works", "bug 1, longest_word: returns the longest word") {
    Words words{"pear", "banana", "fig"};
    std::string r = longest_word(words);
    ZTH_EXPECT_EQ(r, std::string("banana"));
}
ZTH_TEST("works", "bug 2, label_of: returns the label text") {
    std::string r(label_of(7));
    ZTH_EXPECT_EQ(r, label(7));
}
ZTH_TEST("works", "bug 3, remove_evens: keeps the odd numbers in order") {
    ZTH_EXPECT_EQ(remove_evens({1, 2, 3, 4, 5, 6}), (V{1, 3, 5}));
}
ZTH_TEST("works", "bug 4, make_counter: counts from the start value") {
    auto next = make_counter(5);
    ZTH_EXPECT_EQ(next(), 5);
    ZTH_EXPECT_EQ(next(), 6);
    ZTH_EXPECT_EQ(next(), 7);
}
ZTH_TEST("works", "bug 5, use_twice: returns twice the id, with each Widget freed exactly once") {
    zth::mem::Scope scope;
    ZTH_EXPECT_EQ(use_twice(21), 42);
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("works", "bug 6, describe: returns the sentence") {
    std::string r(describe(3));
    ZTH_EXPECT_EQ(r, sentence(3));
    std::string e(describe(4));
    ZTH_EXPECT_EQ(e, sentence(4));
}

// ---- Robust: the fix is real, not a lucky one ---------------------------------------------------------------
ZTH_TEST("robust", "longest_word: the first of several equally long words, and \"\" for none") {
    Words tie{"aa", "bb", "c"};
    std::string first(longest_word(tie));
    ZTH_EXPECT_EQ(first, std::string("aa"));
    Words none;
    std::string empty(longest_word(none));
    ZTH_EXPECT_EQ(empty, std::string(""));
    Words one{"solo"};
    std::string s(longest_word(one));
    ZTH_EXPECT_EQ(s, std::string("solo"));
}
ZTH_TEST("robust", "longest_word: the result stays valid when the word list is gone and after other calls") {
    auto r = []() {
        Words words{"short", "a rather long word indeed", "mid"};
        return std::string(longest_word(words));
    }();
    churn();
    ZTH_EXPECT_EQ(r, std::string("a rather long word indeed"));
}
ZTH_TEST("robust", "longest_word: two results can be kept at the same time") {
    Words a{"one", "three"}, b{"four", "eleven"};
    const auto& ra = longest_word(a);
    const auto& rb = longest_word(b);
    ZTH_EXPECT_EQ(std::string(ra), std::string("three"));
    ZTH_EXPECT_EQ(std::string(rb), std::string("eleven"));
    ZTH_EXPECT_EQ(a.size(), std::size_t{2});
}
ZTH_TEST("robust", "label_of: labels stay valid after other calls, and several can be kept at once") {
    const auto& l1 = label_of(1);
    const auto& l2 = label_of(22);
    churn();
    const auto& l3 = label_of(333);
    ZTH_EXPECT_EQ(std::string(l1), label(1));
    ZTH_EXPECT_EQ(std::string(l2), label(22));
    ZTH_EXPECT_EQ(std::string(l3), label(333));
}
ZTH_TEST("robust", "remove_evens: runs of evens, all evens, no evens, negatives, nothing") {
    ZTH_EXPECT_EQ(remove_evens({2, 4, 6, 8}), V{});
    ZTH_EXPECT_EQ(remove_evens({1, 3, 5}), (V{1, 3, 5}));
    ZTH_EXPECT_EQ(remove_evens({1, 2, 4, 6, 7, 8, 9}), (V{1, 7, 9}));
    ZTH_EXPECT_EQ(remove_evens({-3, -2, -1, 0, 1}), (V{-3, -1, 1}));
    ZTH_EXPECT_EQ(remove_evens({}), V{});
    ZTH_EXPECT_EQ(remove_evens({4}), V{});
    ZTH_EXPECT_EQ(remove_evens({3}), (V{3}));
}
ZTH_TEST("robust", "remove_evens: a long vector, and the caller's vector is not changed") {
    V big;
    for (int i = 0; i < 1000; ++i) big.push_back(i);
    V r = remove_evens(big);
    ZTH_EXPECT_EQ(r.size(), std::size_t{500});
    ZTH_EXPECT_EQ(r.front(), 1);
    ZTH_EXPECT_EQ(r.back(), 999);
    ZTH_EXPECT_EQ(big.size(), std::size_t{1000});
}
ZTH_TEST("robust", "make_counter: the counter still works after the function that made it has returned and the stack was reused") {
    auto next = make_counter(10);
    churn();
    ZTH_EXPECT_EQ(next(), 10);
    churn();
    ZTH_EXPECT_EQ(next(), 11);
    ZTH_EXPECT_EQ(next(), 12);
}
ZTH_TEST("robust", "make_counter: every counter has its own state, and a copy of a counter is independent of the original") {
    auto a = make_counter(0);
    auto b = make_counter(100);
    ZTH_EXPECT_EQ(a(), 0);
    ZTH_EXPECT_EQ(a(), 1);
    ZTH_EXPECT_EQ(b(), 100);
    ZTH_EXPECT_EQ(a(), 2);
    ZTH_EXPECT_EQ(b(), 101);
    auto c = make_counter(-2);
    ZTH_EXPECT_EQ(c(), -2);
    ZTH_EXPECT_EQ(c(), -1);
}
ZTH_TEST("robust", "use_twice: many calls, different ids, nothing leaks and nothing is freed twice") {
    zth::mem::Scope scope;
    for (int id : {0, 1, -5, 1000, 12345}) ZTH_EXPECT_EQ(use_twice(id), 2 * id);
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "describe: odd and even, negative numbers, and several sentences can be kept at once") {
    const auto& a = describe(1);
    const auto& b = describe(2);
    const auto& c = describe(-7);
    churn();
    ZTH_EXPECT_EQ(std::string(a), sentence(1));
    ZTH_EXPECT_EQ(std::string(b), sentence(2));
    ZTH_EXPECT_EQ(std::string(c), sentence(-7));
}

// ---- Clean --------------------------------------------------------------------------------------------------
ZTH_TEST("clean", "main.cpp does not use new, delete, malloc or free (make_shared and the containers own everything)") {
    ZTH_EXPECT_NO_RAW_MEMORY("main.cpp");
}
