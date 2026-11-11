#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "generator.hpp"
#include "zth_test.hpp"

namespace {

// Coroutines written by the TEST, using the learner's Generator: they log what they do, so that the tests can see WHEN the code runs.
Generator<int> noisy(std::string& log) {
    log += "a";
    co_yield 1;
    log += "b";
    co_yield 2;
    log += "c";
}

Generator<int> counting(int& pulls) {
    for (int i = 0;; ++i) {
        ++pulls;
        co_yield i;
    }
}

template <class G>
auto collect(G&& g) {
    std::vector<std::decay_t<decltype(*g.begin())>> out;
    for (auto&& v : g) out.push_back(v);
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "range(3, 8) gives 3 4 5 6 7 in a range-for") {
    std::vector<int> got;
    for (int x : range(3, 8)) got.push_back(x);
    ZTH_EXPECT_EQ(got, (std::vector<int>{3, 4, 5, 6, 7}));
}
ZTH_TEST("works", "an empty range gives nothing: range(5, 5) and range(5, 2)") {
    ZTH_EXPECT_TRUE(collect(range(5, 5)).empty());
    ZTH_EXPECT_TRUE(collect(range(5, 2)).empty());
}
ZTH_TEST("works", "next() and value() work by hand: 1, 2, then false (and false again)") {
    std::string log;
    Generator<int> g = noisy(log);
    ZTH_EXPECT_TRUE(g.next());
    ZTH_EXPECT_EQ(g.value(), 1);
    ZTH_EXPECT_TRUE(g.next());
    ZTH_EXPECT_EQ(g.value(), 2);
    ZTH_EXPECT_FALSE(g.next());
    ZTH_EXPECT_FALSE(g.next());
    ZTH_EXPECT_FALSE(g.next());
}
ZTH_TEST("works", "a generator is LAZY: the body runs only as far as the values that were asked for") {
    std::string log;
    Generator<int> g = noisy(log);
    ZTH_EXPECT_EQ(log, std::string(""));  // created, not started
    g.next();
    ZTH_EXPECT_EQ(log, std::string("a"));
    g.next();
    ZTH_EXPECT_EQ(log, std::string("ab"));
    g.next();
    ZTH_EXPECT_EQ(log, std::string("abc"));
}
ZTH_TEST("works", "fibonacci() is infinite: take(fibonacci(), 12) gives the first twelve numbers") {
    ZTH_EXPECT_EQ(collect(take(fibonacci(), 12)), (std::vector<unsigned long long>{0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89}));
}
ZTH_TEST("works", "words splits at spaces and skips empty words") {
    ZTH_EXPECT_EQ(collect(words("the  quick brown   fox")), (std::vector<std::string>{"the", "quick", "brown", "fox"}));
    ZTH_EXPECT_EQ(collect(words("  padded  ")), (std::vector<std::string>{"padded"}));
    ZTH_EXPECT_TRUE(collect(words("")).empty());
    ZTH_EXPECT_TRUE(collect(words("    ")).empty());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "take is lazy: taking 3 values from an endless source asks the source exactly 3 times") {
    int pulls = 0;
    ZTH_EXPECT_EQ(collect(take(counting(pulls), 3)), (std::vector<int>{0, 1, 2}));
    ZTH_EXPECT_EQ(pulls, 3);
}
ZTH_TEST("robust", "take(source, 0) asks for nothing; take with more than the source has gives what there is") {
    int pulls = 0;
    ZTH_EXPECT_TRUE(collect(take(counting(pulls), 0)).empty());
    ZTH_EXPECT_EQ(pulls, 0);
    ZTH_EXPECT_EQ(collect(take(range(0, 3), 10)), (std::vector<int>{0, 1, 2}));
}
ZTH_TEST("robust", "abandoning a generator half way frees its frame: no leak") {
    ::zth::mem::Scope scope;
    {
        Generator<int> g = range(0, 1000000);
        g.next();
        g.next();
    }  // destroyed with a million values still to come
    {
        Generator<int> never_started = range(0, 10);
        (void)never_started;
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a generator that ran to the end is cleaned up too (and so is an empty one)") {
    ::zth::mem::Scope scope;
    {
        Generator<int> g = range(0, 3);
        while (g.next()) {
        }
        Generator<int> empty;
        ZTH_EXPECT_FALSE(empty.next());
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "moving a generator transfers ownership: the new owner continues, the old one is empty, nothing is destroyed twice") {
    ::zth::mem::Scope scope;
    {
        Generator<int> a = range(10, 15);
        ZTH_EXPECT_TRUE(a.next());
        Generator<int> b = std::move(a);
        ZTH_EXPECT_FALSE(a.next());  // moved from: empty
        ZTH_EXPECT_TRUE(b.next());
        ZTH_EXPECT_EQ(b.value(), 11);
        Generator<int> c = range(100, 200);  // a target that owns a frame of its own: move assignment must free it
        ZTH_EXPECT_TRUE(c.next());
        c = std::move(b);
        ZTH_EXPECT_TRUE(c.next());
        ZTH_EXPECT_EQ(c.value(), 12);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a generator can be returned from a function and passed on (it is move-only)") {
    auto make = [] { return range(1, 4); };
    Generator<int> g = make();
    ZTH_EXPECT_EQ(collect(g), (std::vector<int>{1, 2, 3}));
}
ZTH_TEST("robust", "words keeps its own copy of the text: the argument can be a temporary that is gone before the first word is asked for") {
    Generator<std::string> g = [] { return words(std::string("alpha beta ") + "gamma"); }();
    ZTH_EXPECT_EQ(collect(g), (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "generator.hpp has no raw new/delete: the frame is owned through the coroutine handle") {
    ZTH_EXPECT_NO_RAW_MEMORY("generator.hpp");
}
ZTH_TEST("clean", "the four generator functions are coroutines, with co_yield or co_return (not ordinary functions returning an empty generator)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("generator.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "co_yield").size() >= 4);
    ZTH_EXPECT_TRUE(::zth::find_word(code, "co_return").size() >= 1);
}
ZTH_TEST("clean", "coroutine parameters are taken by value: no `const std::string&` parameter on words()") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("generator.hpp"));
    ZTH_EXPECT_TRUE(code.find("words(const") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find("words(std::string_view") == std::string::npos);
}
