#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "max_by.hpp"
#include "zth_test.hpp"

namespace {

// ---- can max_by be called like this? (false, not a compile error in this hidden file, when the template is too narrow)
template <class R, class P>
constexpr bool callable2 = requires(R&& r, P p) { max_by(std::forward<R>(r), p); };
template <class R, class P, class C>
constexpr bool callable3 = requires(R&& r, P p, C c) { max_by(std::forward<R>(r), p, c); };

struct Person {
    std::string name;
    int age;
};

// the position of the element max_by chose, or -1 if it chose end(); fails the test with a clear message if it does not compile
template <class R, class P>
bool check_index(const char* what, R& r, P proj, std::ptrdiff_t expected) {
    if constexpr (!callable2<R&, P>) {
        ::zth::fail(std::string("max_by(") + what + ") does not compile: max_by must be a template that accepts this kind of range and projection");
        return false;
    } else {
        auto it = max_by(r, proj);
        const std::ptrdiff_t got = it == std::end(r) ? -1 : std::distance(std::begin(r), it);
        if (got != expected) {
            ::zth::fail(std::string("max_by(") + what + ") chose position " + std::to_string(got) + " but should choose " + std::to_string(expected) +
                        " (-1 means end())");
            return false;
        }
        return true;
    }
}

template <class R, class P, class C>
bool check_index3(const char* what, R& r, P proj, C comp, std::ptrdiff_t expected) {
    if constexpr (!callable3<R&, P, C>) {
        ::zth::fail(std::string("max_by(") + what + ") does not compile: max_by needs an optional third parameter, a comparison of the keys (default std::less<>)");
        return false;
    } else {
        auto it = max_by(r, proj, comp);
        const std::ptrdiff_t got = it == std::end(r) ? -1 : std::distance(std::begin(r), it);
        if (got != expected) {
            ::zth::fail(std::string("max_by(") + what + ") chose position " + std::to_string(got) + " but should choose " + std::to_string(expected));
            return false;
        }
        return true;
    }
}

auto identity = [](int x) { return x; };
auto length = [](const std::string& s) { return s.size(); };

// a key type that has ONLY operator< (no >, no ==)
struct Key {
    int v;
    bool operator<(const Key& o) const { return v < o.v; }
};
struct Item {
    Key k;
    int tag;
};

template <class R, class P>
void iterator_type_test(R& r, P proj, const char* what) {
    if constexpr (!callable2<R&, P>) {
        ::zth::fail(std::string("max_by(") + what + ") does not compile");
    } else {
        using Result = decltype(max_by(r, proj));
        ZTH_EXPECT_TRUE((std::is_same_v<Result, decltype(std::begin(r))>));
    }
}

template <class R, class P>
void write_through_test(R& r, P proj) {
    if constexpr (!callable2<R&, P>) {
        ::zth::fail("max_by does not compile for a non-const vector");
    } else {
        *max_by(r, proj) = -1;
        ZTH_EXPECT_EQ(r[1], -1);
    }
}

template <class R, class P>
void projection_calls_test(R& r, P counting, const int& calls, int expected_calls, int expected_max) {
    if constexpr (!callable2<R&, P&>) {
        ::zth::fail("max_by does not compile with a lambda that captures by reference");
    } else {
        auto it = max_by(r, counting);
        ZTH_EXPECT_EQ(*it, expected_max);
        ZTH_EXPECT_EQ(calls, expected_calls);
    }
}

template <class R, class P, class C>
void comparison_calls_test(R& r, P proj, C counting_less, const int& compares, int expected_compares) {
    if constexpr (!callable3<R&, P&, C&>) {
        ::zth::fail("max_by does not compile with a third argument");
    } else {
        (void)max_by(r, proj, counting_less);
        ZTH_EXPECT_EQ(compares, expected_compares);
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "finds the largest element of a vector, projecting each element to a key") {
    std::vector<int> v{3, 9, 2, 5};
    check_index("vector<int>, identity", v, identity, 1);
    std::vector<std::string> words{"pear", "fig", "banana", "kiwi"};
    check_index("vector<string>, length", words, length, 2);
}
ZTH_TEST("works", "a member pointer works as the projection") {
    std::vector<Person> people{{"Ada", 36}, {"Grace", 45}, {"Alan", 41}};
    check_index("vector<Person>, &Person::age", people, &Person::age, 1);
    check_index("vector<Person>, &Person::name", people, &Person::name, 1);  // the name that sorts last: Grace
}
ZTH_TEST("works", "the largest of equal keys is the FIRST one") {
    std::vector<int> v{7, 9, 3, 9, 9};
    check_index("vector<int> with ties", v, identity, 1);
    std::vector<std::string> words{"ab", "cd", "e"};
    check_index("ties by length", words, length, 0);
}
ZTH_TEST("works", "an empty range gives end()") {
    std::vector<int> v;
    check_index("empty vector", v, identity, -1);
    std::list<std::string> l;
    check_index("empty list", l, length, -1);
}
ZTH_TEST("works", "works with std::array, a C array and a std::list") {
    std::array<int, 4> a{4, 1, 8, 2};
    check_index("std::array", a, identity, 2);
    int c[] = {5, 6, 99, 7};
    check_index("C array", c, identity, 2);
    std::list<int> l{1, 3, 2};
    check_index("std::list", l, identity, 1);
}
ZTH_TEST("works", "a const range gives a const iterator, a non-const one an iterator you can write through") {
    const std::vector<int> cv{1, 5, 2};
    iterator_type_test(cv, identity, "const vector");
    std::vector<int> v{1, 5, 2};
    iterator_type_test(v, identity, "vector");
    write_through_test(v, identity);
}
ZTH_TEST("works", "the optional third argument compares the keys (std::greater<> finds the smallest)") {
    std::vector<int> v{4, 2, 8, 2, 6};
    check_index3("vector<int>, identity, greater", v, identity, std::greater<>{}, 1);
    std::vector<std::string> words{"pear", "fig", "banana", "kiwi"};
    check_index3("words, length, greater", words, length, std::greater<>{}, 1);
    check_index3("words, length, less", words, length, std::less<>{}, 2);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "projections may return different key types: double, std::string, a pair, bool") {
    std::vector<double> d{1.5, -2.0, 9.25, 9.0};
    check_index("doubles", d, [](double x) { return x; }, 2);
    std::vector<std::string> s{"b", "a", "c", "c"};
    check_index("strings as keys", s, [](const std::string& x) { return x; }, 2);
    std::vector<std::pair<int, int>> p{{1, 9}, {2, 1}, {2, 5}, {0, 100}};
    check_index("pairs (lexicographic)", p, [](const std::pair<int, int>& x) { return x; }, 2);
    std::vector<int> b{0, 0, 7, 0};
    check_index("bool keys", b, [](int x) { return x != 0; }, 2);
}
ZTH_TEST("robust", "a projection that returns a reference into the element works") {
    std::vector<Person> people{{"Zed", 20}, {"Amy", 30}};
    check_index("projection returning const std::string&", people, [](const Person& p) -> const std::string& { return p.name; }, 0);
}
ZTH_TEST("robust", "elements that cannot be copied (std::unique_ptr) are fine") {
    std::vector<std::unique_ptr<int>> v;
    v.push_back(std::make_unique<int>(3));
    v.push_back(std::make_unique<int>(11));
    v.push_back(std::make_unique<int>(7));
    check_index("vector<unique_ptr<int>>", v, [](const std::unique_ptr<int>& p) { return *p; }, 1);
}
ZTH_TEST("robust", "the elements themselves are never copied or moved") {
    std::vector<zth::Tracked> v{3, 8, 2, 8};
    zth::Tracked::reset();
    check_index("vector<Tracked>", v, [](const zth::Tracked& t) { return t.value; }, 1);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("robust", "it only needs operator< on the keys (no >, no ==)") {
    std::vector<Item> v{{{3}, 0}, {{9}, 1}, {{9}, 2}, {{1}, 3}};
    check_index("keys with only operator<", v, [](const Item& i) { return i.k; }, 1);
}
ZTH_TEST("robust", "a single element, all equal elements, and a long range") {
    std::vector<int> one{42};
    check_index("one element", one, identity, 0);
    std::vector<int> same(10, 5);
    check_index("all equal", same, identity, 0);
    std::vector<int> big(100000, 1);
    big[77777] = 2;
    check_index("100000 elements", big, identity, 77777);
}
ZTH_TEST("robust", "the comparison decides (a stateless function object and a lambda both work)") {
    std::vector<int> v{5, 1, 9, 3};
    check_index3("greater<>", v, identity, std::greater<>{}, 1);
    check_index3("lambda: compare by distance from 4", v, identity, [](int a, int b) { return std::abs(a - 4) < std::abs(b - 4); }, 2);
}

// ---- Fast: the projection is called exactly once per element ----------------------------------------------
ZTH_TEST("fast", "the projection is called once per element (not twice: the best key is remembered)") {
    std::vector<int> v(1000);
    for (int i = 0; i < 1000; ++i) v[static_cast<std::size_t>(i)] = (i * 37) % 1000;
    int calls = 0;
    projection_calls_test(v, [&](int x) {
        ++calls;
        return x;
    }, calls, 1000, 999);
}
ZTH_TEST("fast", "the comparison is called once per element after the first") {
    std::vector<int> v(500, 1);
    int compares = 0;
    comparison_calls_test(v, identity, [&](int a, int b) {
        ++compares;
        return a < b;
    }, compares, 499);
}
