#define ZTH_TRACK_ALLOCATIONS
#include <array>
#include <cstddef>
#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pretty.hpp"
#include "zth_test.hpp"

namespace {

struct Point {
    int x, y;
};
std::ostream& operator<<(std::ostream& os, const Point& p) { return os << "P(" << p.x << "," << p.y << ")"; }

struct Opaque {  // no operator<<
    int v;
};

using S = std::string;

// the text for x, as a std::string, for the macros below
template <class T>
S text(const T& x) {
    return to_text(x);
}

}  // namespace

// ---- Works: to_text --------------------------------------------------------------------------------
ZTH_TEST("works", "bool prints as true / false, not 1 / 0") {
    ZTH_EXPECT_EQ(text(true), S("true"));
    ZTH_EXPECT_EQ(text(false), S("false"));
}
ZTH_TEST("works", "integers print in decimal, signed and unsigned") {
    ZTH_EXPECT_EQ(text(42), S("42"));
    ZTH_EXPECT_EQ(text(-7), S("-7"));
    ZTH_EXPECT_EQ(text(0u), S("0"));
    ZTH_EXPECT_EQ(text(123456789012LL), S("123456789012"));
    ZTH_EXPECT_EQ(text(static_cast<unsigned short>(65535)), S("65535"));
}
ZTH_TEST("works", "a char prints in single quotes") {
    ZTH_EXPECT_EQ(text('a'), S("'a'"));
    ZTH_EXPECT_EQ(text(' '), S("' '"));
}
ZTH_TEST("works", "floating point prints as `os << x` does: 1.5, 2, 0.1") {
    ZTH_EXPECT_EQ(text(1.5), S("1.5"));
    ZTH_EXPECT_EQ(text(2.0), S("2"));
    ZTH_EXPECT_EQ(text(0.1), S("0.1"));
    ZTH_EXPECT_EQ(text(-3.25f), S("-3.25"));
}
ZTH_TEST("works", "strings print in double quotes: std::string, std::string_view, const char* and literals") {
    ZTH_EXPECT_EQ(text(S("hello")), S("\"hello\""));
    ZTH_EXPECT_EQ(text(std::string_view("view")), S("\"view\""));
    const char* c = "cstr";
    ZTH_EXPECT_EQ(text(c), S("\"cstr\""));
    ZTH_EXPECT_EQ(to_text("literal"), S("\"literal\""));
    ZTH_EXPECT_EQ(text(S("")), S("\"\""));
}
ZTH_TEST("works", "a std::pair prints as (first, second), and each part is printed by its own type") {
    ZTH_EXPECT_EQ(text(std::make_pair(1, S("one"))), S("(1, \"one\")"));
    ZTH_EXPECT_EQ(text(std::make_pair('x', true)), S("('x', true)"));
}
ZTH_TEST("works", "a std::optional prints as none or some(x)") {
    ZTH_EXPECT_EQ(text(std::optional<int>{}), S("none"));
    ZTH_EXPECT_EQ(text(std::optional<int>{5}), S("some(5)"));
    ZTH_EXPECT_EQ(text(std::optional<S>{"x"}), S("some(\"x\")"));
}
ZTH_TEST("works", "a range prints as [a, b, c], and [] when empty: vector, std::array, std::list, a C array") {
    ZTH_EXPECT_EQ(text(std::vector<int>{1, 2, 3}), S("[1, 2, 3]"));
    ZTH_EXPECT_EQ(text(std::vector<int>{}), S("[]"));
    ZTH_EXPECT_EQ(text(std::vector<S>{"a", "b"}), S("[\"a\", \"b\"]"));
    ZTH_EXPECT_EQ(text(std::array<int, 2>{7, 8}), S("[7, 8]"));
    ZTH_EXPECT_EQ(text(std::list<bool>{true, false}), S("[true, false]"));
    int arr[] = {4, 5, 6};
    ZTH_EXPECT_EQ(text(arr), S("[4, 5, 6]"));
}
ZTH_TEST("works", "nested: a vector of vectors, a vector of pairs, a vector of optionals") {
    std::vector<std::vector<int>> nested{{1}, {2, 3}, {}};
    ZTH_EXPECT_EQ(text(nested), S("[[1], [2, 3], []]"));
    std::vector<std::pair<int, S>> pairs{{1, "a"}, {2, "b"}};
    ZTH_EXPECT_EQ(text(pairs), S("[(1, \"a\"), (2, \"b\")]"));
    std::vector<std::optional<int>> opts{1, std::nullopt, 3};
    ZTH_EXPECT_EQ(text(opts), S("[some(1), none, some(3)]"));
}
ZTH_TEST("works", "a std::map prints as a range of pairs: a pair overload must be found from inside the generic function") {
    std::map<int, S> m{{1, "a"}, {2, "b"}};
    ZTH_EXPECT_EQ(text(m), S("[(1, \"a\"), (2, \"b\")]"));
    std::map<S, std::vector<int>> scores{{"ada", {90, 85}}, {"bob", {}}};
    ZTH_EXPECT_EQ(text(scores), S("[(\"ada\", [90, 85]), (\"bob\", [])]"));
}
ZTH_TEST("works", "pointers print as nullptr or & and the value they point to; nullptr itself prints as nullptr") {
    int n = 5;
    int* p = &n;
    int* none = nullptr;
    ZTH_EXPECT_EQ(text(p), S("&5"));
    ZTH_EXPECT_EQ(text(none), S("nullptr"));
    ZTH_EXPECT_EQ(text(nullptr), S("nullptr"));
    S s = "txt";
    S* sp = &s;
    ZTH_EXPECT_EQ(text(sp), S("&\"txt\""));
}
ZTH_TEST("works", "a type with its own operator<< uses it; a type without one prints as <unprintable>") {
    ZTH_EXPECT_EQ(text(Point{1, 2}), S("P(1,2)"));
    ZTH_EXPECT_EQ(text(Opaque{1}), S("<unprintable>"));
    ZTH_EXPECT_EQ(text(std::vector<Point>{{1, 2}, {3, 4}}), S("[P(1,2), P(3,4)]"));
    ZTH_EXPECT_EQ(text(std::vector<Opaque>{{1}}), S("[<unprintable>]"));
}

// ---- Works: TypeName ---------------------------------------------------------------------------------
ZTH_TEST("works", "TypeName has a full specialization for int, double, bool and std::string, and 'unknown' for the rest") {
    ZTH_EXPECT_EQ(TypeName<int>::name(), S("int"));
    ZTH_EXPECT_EQ(TypeName<double>::name(), S("double"));
    ZTH_EXPECT_EQ(TypeName<bool>::name(), S("bool"));
    ZTH_EXPECT_EQ(TypeName<S>::name(), S("string"));
    ZTH_EXPECT_EQ(TypeName<long>::name(), S("unknown"));
    ZTH_EXPECT_EQ(TypeName<Point>::name(), S("unknown"));
}
ZTH_TEST("works", "partial specializations for pointers, const, vectors and pairs build on the names of their parts") {
    ZTH_EXPECT_EQ(TypeName<int*>::name(), S("pointer to int"));
    ZTH_EXPECT_EQ(TypeName<double*>::name(), S("pointer to double"));
    ZTH_EXPECT_EQ(TypeName<const int>::name(), S("const int"));
    ZTH_EXPECT_EQ(TypeName<std::vector<S>>::name(), S("vector of string"));
    using P = std::pair<int, S>;
    ZTH_EXPECT_EQ(TypeName<P>::name(), S("pair of int and string"));
}

// ---- Robust ------------------------------------------------------------------------------------------
ZTH_TEST("robust", "a signed or unsigned char is a number, only plain char is quoted") {
    ZTH_EXPECT_EQ(text(static_cast<unsigned char>(65)), S("65"));
    ZTH_EXPECT_EQ(text(static_cast<signed char>(-3)), S("-3"));
    ZTH_EXPECT_EQ(text(static_cast<char>(65)), S("'A'"));
}
ZTH_TEST("robust", "deeply nested values: a map of vectors of optionals, a pair of pairs") {
    std::map<S, std::vector<std::optional<int>>> m{{"a", {1, std::nullopt}}};
    ZTH_EXPECT_EQ(text(m), S("[(\"a\", [some(1), none])]"));
    auto pp = std::make_pair(std::make_pair(1, 2), std::make_pair(S("x"), false));
    ZTH_EXPECT_EQ(text(pp), S("((1, 2), (\"x\", false))"));
}
ZTH_TEST("robust", "temporaries and const values print too, and a long range is printed completely") {
    ZTH_EXPECT_EQ(text(std::vector<int>{1, 2}), S("[1, 2]"));
    const std::vector<int> cv{9, 8};
    ZTH_EXPECT_EQ(text(cv), S("[9, 8]"));
    const int ci = 3;
    ZTH_EXPECT_EQ(text(ci), S("3"));
    std::vector<int> big(1000, 7);
    const S t = text(big);
    ZTH_EXPECT_EQ(t.size(), std::size_t{2 + 1000 * 1 + 999 * 2});
    ZTH_EXPECT_EQ(t.substr(0, 7), S("[7, 7, "));
}
ZTH_TEST("robust", "a pointer to a pointer, a pointer to a vector, an optional of a vector") {
    int n = 1;
    int* p = &n;
    int** pp = &p;
    ZTH_EXPECT_EQ(text(pp), S("&&1"));
    std::vector<int> v{1, 2};
    ZTH_EXPECT_EQ(text(&v), S("&[1, 2]"));
    ZTH_EXPECT_EQ(text(std::optional<std::vector<int>>{std::vector<int>{4}}), S("some([4])"));
}
ZTH_TEST("robust", "TypeName composes: vector of pointer to const int, pair of pairs") {
    ZTH_EXPECT_EQ(TypeName<std::vector<const int*>>::name(), S("vector of pointer to const int"));
    using Inner = std::pair<int, bool>;
    using Outer = std::pair<Inner, double>;
    ZTH_EXPECT_EQ(TypeName<Outer>::name(), S("pair of pair of int and bool and double"));
    ZTH_EXPECT_EQ(TypeName<std::vector<std::vector<int>>>::name(), S("vector of vector of int"));
    ZTH_EXPECT_EQ(TypeName<int**>::name(), S("pointer to pointer to int"));
}

// ---- Fast --------------------------------------------------------------------------------------------
ZTH_TEST("fast", "printing a long vector builds the text with one growing string, not a new string per element") {
    std::vector<int> big(20000, 12345);
    zth::mem::Scope scope;
    const S t = to_text(big);
    ZTH_EXPECT_EQ(t.size(), std::size_t{2 + 20000 * 5 + 19999 * 2});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 60);  // the string doubles its capacity about log2(n) times
}
