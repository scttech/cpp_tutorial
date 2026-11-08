#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "variadic.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

// ---- can this be called with these arguments? (a plain stub taking ONE argument is "false", not a compile error in this hidden file) ----
template <class... Ts> constexpr bool can_count = requires { count_of(std::declval<const Ts&>()...); };
template <class... Ts> constexpr bool can_sum = requires { sum(std::declval<const Ts&>()...); };
template <class... Ts> constexpr bool can_all_true = requires { all_true(std::declval<const Ts&>()...); };
template <class... Ts> constexpr bool can_any_true = requires { any_true(std::declval<const Ts&>()...); };
template <class... Ts> constexpr bool can_all_equal = requires { all_equal(std::declval<const Ts&>()...); };
template <class... Ts> constexpr bool can_format = requires { interpolate(std::declval<const char*>(), std::declval<const Ts&>()...); };
template <class T, class... Args> constexpr bool can_make = requires { make_owned<T>(std::declval<Args>()...); };
template <class... Args> constexpr bool can_for_each = requires { for_each_arg(std::declval<Args>()...); };

// ---- evaluated at COMPILE time? (a failure to be a constant expression makes these false) ----
template <std::size_t N, class... Ts> constexpr bool count_is_constant = requires { typename std::enable_if_t<(count_of(Ts{}...) == N)>; };
template <int N, int... Is> constexpr bool sum_is_constant = requires { typename std::enable_if_t<(sum(Is...) == N)>; };
template <bool B, bool... Bs> constexpr bool all_true_is_constant = requires { typename std::enable_if_t<(all_true(Bs...) == B)>; };
template <bool B, bool... Bs> constexpr bool any_true_is_constant = requires { typename std::enable_if_t<(any_true(Bs...) == B)>; };
template <bool B, int... Is> constexpr bool all_equal_is_constant = requires { typename std::enable_if_t<(all_equal(Is...) == B)>; };

template <class... Ts>
void count_test(std::size_t expected, const Ts&... values) {
    if constexpr (!can_count<Ts...>) {
        ::zth::fail("count_of does not accept " + std::to_string(sizeof...(Ts)) + " argument(s): make it a variadic template (class... Ts)");
    } else {
        ZTH_EXPECT_EQ(count_of(values...), expected);
    }
}

// sum(vals...) must be exactly `expected` AND have exactly the type of `expected`
template <class Expected, class... Ts>
void sum_test(Expected expected, Ts... vals) {
    if constexpr (!can_sum<Ts...>) {
        ::zth::fail("sum does not accept " + std::to_string(sizeof...(Ts)) + " argument(s)");
    } else {
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(sum(vals...)), Expected>));
        ZTH_EXPECT_TRUE(sum(vals...) == expected);
    }
}

template <class... Ts>
void all_true_test(bool expected, Ts... vals) {
    if constexpr (!can_all_true<Ts...>) {
        ::zth::fail("all_true does not accept " + std::to_string(sizeof...(Ts)) + " argument(s)");
    } else {
        ZTH_EXPECT_EQ(all_true(vals...), expected);
    }
}

template <class... Ts>
void any_true_test(bool expected, Ts... vals) {
    if constexpr (!can_any_true<Ts...>) {
        ::zth::fail("any_true does not accept " + std::to_string(sizeof...(Ts)) + " argument(s)");
    } else {
        ZTH_EXPECT_EQ(any_true(vals...), expected);
    }
}

template <class... Ts>
void all_equal_test(bool expected, const Ts&... vals) {
    if constexpr (!can_all_equal<Ts...>) {
        ::zth::fail("all_equal does not accept " + std::to_string(sizeof...(Ts)) + " argument(s)");
    } else {
        ZTH_EXPECT_EQ(all_equal(vals...), expected);
    }
}

template <class... Ts>
void format_test(const char* fmt, const char* expected, const Ts&... vals) {
    if constexpr (!can_format<Ts...>) {
        ::zth::fail("interpolate does not accept " + std::to_string(sizeof...(Ts)) + " value argument(s)");
    } else {
        ZTH_EXPECT_EQ(interpolate(fmt, vals...), S(expected));
    }
}

// ---- types for the forwarding tests ----
struct Probe {  // remembers which constructor it was built with
    S s;
    S how;
    Probe() : how("none") {}
    Probe(const S& x) : s(x), how("copy") {}  // NOLINT
    Probe(S&& x) : s(std::move(x)), how("move") {}  // NOLINT
    Probe(const S& x, int n) : s(x + std::to_string(n)), how("copy") {}
    Probe(S&& x, int n) : s(std::move(x) + std::to_string(n)), how("move") {}
};

struct Holder {
    zth::Tracked t;
    explicit Holder(const zth::Tracked& x) : t(x) {}
    explicit Holder(zth::Tracked&& x) : t(std::move(x)) {}
};

struct Pair3 {
    int a;
    double b;
    S c;
    Pair3(int x, double y, S z) : a(x), b(y), c(std::move(z)) {}
};

struct Recorder {  // what for_each_arg passed: its value category and its text
    std::vector<S> seen;
    template <class X>
    void operator()(X&& x) {
        std::ostringstream os;
        os << (std::is_lvalue_reference_v<X> ? "L:" : "R:") << x;
        seen.push_back(os.str());
    }
};

// ---- calls that report a missing/unsuitable signature instead of failing to compile ----
template <class T, class... Args>
std::unique_ptr<T> make_or_fail(Args&&... args) {
    if constexpr (!can_make<T, Args...>) {
        ::zth::fail("make_owned<T>(args...) does not accept " + std::to_string(sizeof...(Args)) + " argument(s) of these types");
        return nullptr;
    } else {
        return make_owned<T>(std::forward<Args>(args)...);
    }
}

template <class F, class... Args>
void each_or_fail(F&& f, Args&&... args) {
    if constexpr (!can_for_each<F, Args...>) {
        ::zth::fail("for_each_arg(f, args...) does not accept " + std::to_string(sizeof...(Args)) + " argument(s) of these types");
    } else {
        for_each_arg(std::forward<F>(f), std::forward<Args>(args)...);
    }
}

template <class Fmt, class... Ts>
std::string interpolate_or_fail(const Fmt& fmt, const Ts&... vals) {
    if constexpr (!requires { interpolate(fmt, vals...); }) {
        ::zth::fail("interpolate does not accept this kind of format string");
        return "";
    } else {
        return interpolate(fmt, vals...);
    }
}

}  // namespace

// ---- Works ----------------------------------------------------------------------------------------------
ZTH_TEST("works", "count_of counts its arguments, whatever their types") {
    count_test<>(0);
    count_test<int>(1, 5);
    count_test<int, const char*, double>(3, 1, "a", 2.5);
    count_test<S, S, S, S>(4, S("a"), S("b"), S("c"), S("d"));
}
ZTH_TEST("works", "sum adds integers") {
    sum_test<int>(6, 1, 2, 3);
    sum_test<int>(5, 5);
    sum_test<int>(-3, 4, -10, 3);
}
ZTH_TEST("works", "sum of doubles, and of mixed types (the common type)") {
    sum_test<double>(3.5, 1, 2.5);
    sum_test<double>(0.75, 0.25, 0.5);
    sum_test<double>(6.5, 1, 2, 3.5);
    sum_test<long long>((1LL << 40) + 1, 1LL << 40, 1);
}
ZTH_TEST("works", "all_true: every argument is true") {
    all_true_test(true, true, true);
    all_true_test(false, true, false, true);
    all_true_test(true, 1, 2 > 1, 3);
    all_true_test(false, 1, 0);
}
ZTH_TEST("works", "any_true: at least one argument is true") {
    any_true_test(true, false, true, false);
    any_true_test(false, false, false);
    any_true_test(true, 0, 0, 7);
    any_true_test(false, 0, 0);
}
ZTH_TEST("works", "all_equal compares every following argument with the first") {
    all_equal_test(true, 1, 1, 1);
    all_equal_test(false, 1, 1, 2);
    all_equal_test(false, 1, 2, 2);
    all_equal_test(false, 2, 1, 1);
    all_equal_test(true, S("a"), S("a"));
    all_equal_test(false, S("a"), S("b"));
}
ZTH_TEST("works", "all_equal accepts arguments of different types") {
    all_equal_test(true, 1, 1.0, 1L, 1.0f);
    all_equal_test(false, 1, 1.5);
    all_equal_test(true, S("ab"), "ab", S("ab"));
    all_equal_test(false, S("ab"), "ac");
}
ZTH_TEST("works", "interpolate replaces each {} with the next argument") {
    format_test("{} + {} = {}", "1 + 2 = 3", 1, 2, 3);
    format_test("hello {}!", "hello world!", S("world"));
    format_test("{}", "x", S("x"));
    format_test("no placeholders", "no placeholders");
}
ZTH_TEST("works", "interpolate writes any printable type with <<") {
    format_test("{}: {}", "pi: 3.14", "pi", 3.14);
    format_test("{} {} {}", "a 7 2.5", 'a', 7L, 2.5f);
    format_test("[{}]", "[abc]", S("abc"));
    format_test("{}", "1", true);
}
ZTH_TEST("works", "make_owned builds a T from its constructor arguments") {
    auto p = make_or_fail<Pair3>(1, 2.5, S("c"));
    if (p) {
        ZTH_EXPECT_EQ(p->a, 1);
        ZTH_EXPECT_NEAR(p->b, 2.5, 1e-12);
        ZTH_EXPECT_EQ(p->c, S("c"));
    }
    auto q = make_or_fail<Probe>();
    if (q) ZTH_EXPECT_EQ(q->how, S("none"));
}
ZTH_TEST("works", "for_each_arg calls the function on each argument, in order") {
    Recorder rec;
    each_or_fail(rec, 1, "two", 3.5);
    ZTH_EXPECT_EQ(rec.seen.size(), std::size_t{3});
    if (rec.seen.size() == 3) {
        ZTH_EXPECT_EQ(rec.seen[0], S("R:1"));
        ZTH_EXPECT_EQ(rec.seen[1].substr(2), S("two"));
        ZTH_EXPECT_EQ(rec.seen[2], S("R:3.5"));
    }
}
ZTH_TEST("works", "for_each_arg with a lambda, and with no arguments") {
    Recorder rec;
    each_or_fail(rec);
    ZTH_EXPECT_TRUE(rec.seen.empty());
    int total = 0;
    auto add = [&](int x) { total += x; };
    each_or_fail(add, 1, 2, 3);
    ZTH_EXPECT_EQ(total, 6);
}


// ---- Robust ---------------------------------------------------------------------------------------------
ZTH_TEST("robust", "the empty pack: sum() is 0, all_true() is true, any_true() is false") {
    sum_test<int>(0);
    all_true_test(true);
    any_true_test(false);
}
ZTH_TEST("robust", "one argument: sum(x) is x, all_equal(x) is true") {
    sum_test<double>(2.5, 2.5);
    all_true_test(false, 0);
    all_true_test(true, 5);
    any_true_test(true, 5);
    any_true_test(false, 0);
    all_equal_test(true, 42);
}
ZTH_TEST("robust", "the folds can be evaluated at compile time (constexpr)") {
    ZTH_EXPECT_TRUE((count_is_constant<3, int, int, int>));
    ZTH_EXPECT_TRUE((count_is_constant<0>));
    ZTH_EXPECT_TRUE((sum_is_constant<6, 1, 2, 3>));
    ZTH_EXPECT_TRUE((sum_is_constant<0>));
    ZTH_EXPECT_TRUE((all_true_is_constant<true, true, true>));
    ZTH_EXPECT_TRUE((all_true_is_constant<false, true, false>));
    ZTH_EXPECT_TRUE((any_true_is_constant<true, false, true>));
    ZTH_EXPECT_TRUE((any_true_is_constant<false>));
    ZTH_EXPECT_TRUE((all_equal_is_constant<true, 2, 2, 2>));
    ZTH_EXPECT_TRUE((all_equal_is_constant<false, 2, 3>));
}
ZTH_TEST("robust", "interpolate: more arguments than {} -> the extra ones are ignored") {
    format_test("a {} b", "a 1 b", 1, 2, 3);
    format_test("none", "none", 1, 2);
    format_test("", "", 1);
}
ZTH_TEST("robust", "interpolate: more {} than arguments -> the rest stay as {}") {
    format_test("{} {}", "1 {}", 1);
    format_test("{} and {}", "{} and {}");
    format_test("x{}y{}z", "x7y{}z", 7);
}
ZTH_TEST("robust", "interpolate: adjacent placeholders, and text before, between and after") {
    format_test("{}{}", "12", 1, 2);
    format_test("{}{}{}", "abc", 'a', 'b', 'c');
    format_test("<{}>", "<5>", 5);
    format_test("tail: {} end", "tail: 9 end", 9);
    format_test("x={}!", "x=5!", 5);
}
ZTH_TEST("robust", "interpolate: single braces are plain text, and an argument containing {} is not expanded again") {
    format_test("{ } and { {}", "{ } and { 1", 1);
    format_test("} {}", "} 1", 1);
    format_test("{} {}", "{} 1", S("{}"), 1);
}
ZTH_TEST("robust", "interpolate works with std::string and std::string_view format strings") {
    ZTH_EXPECT_EQ(interpolate_or_fail(S("{}-{}"), 1, 2), S("1-2"));
    ZTH_EXPECT_EQ(interpolate_or_fail(std::string_view("{}"), 3), S("3"));
}
ZTH_TEST("robust", "make_owned keeps the value category: an rvalue std::string is MOVED, an lvalue is COPIED") {
    auto from_rvalue = make_or_fail<Probe>(S("temp"));
    if (from_rvalue) {
        ZTH_EXPECT_EQ(from_rvalue->how, S("move"));
        ZTH_EXPECT_EQ(from_rvalue->s, S("temp"));
    }
    S name = "name";
    auto from_lvalue = make_or_fail<Probe>(name);
    if (from_lvalue) ZTH_EXPECT_EQ(from_lvalue->how, S("copy"));
    ZTH_EXPECT_EQ(name, S("name"));  // an lvalue is left alone
    const S fixed = "fixed";
    auto from_const = make_or_fail<Probe>(fixed);
    if (from_const) ZTH_EXPECT_EQ(from_const->how, S("copy"));
    S gone = "gone";
    auto moved = make_or_fail<Probe>(std::move(gone));
    if (moved) ZTH_EXPECT_EQ(moved->how, S("move"));
}
ZTH_TEST("robust", "make_owned forwards EACH argument with its own category") {
    auto a = make_or_fail<Probe>(S("x"), 1);
    if (a) {
        ZTH_EXPECT_EQ(a->how, S("move"));
        ZTH_EXPECT_EQ(a->s, S("x1"));
    }
    S name = "y";
    auto b = make_or_fail<Probe>(name, 2);
    if (b) {
        ZTH_EXPECT_EQ(b->how, S("copy"));
        ZTH_EXPECT_EQ(b->s, S("y2"));
    }
}
ZTH_TEST("robust", "for_each_arg keeps the value category too: lvalues arrive as lvalues, rvalues as rvalues") {
    Recorder rec;
    int a = 1;
    const int b = 2;
    S text = "t";
    each_or_fail(rec, a, b, 3, S("u"), std::move(text), 4.5);
    ZTH_EXPECT_EQ(rec.seen.size(), std::size_t{6});
    if (rec.seen.size() == 6) {
        ZTH_EXPECT_EQ(rec.seen[0], S("L:1"));
        ZTH_EXPECT_EQ(rec.seen[1], S("L:2"));
        ZTH_EXPECT_EQ(rec.seen[2], S("R:3"));
        ZTH_EXPECT_EQ(rec.seen[3], S("R:u"));
        ZTH_EXPECT_EQ(rec.seen[4], S("R:t"));
        ZTH_EXPECT_EQ(rec.seen[5], S("R:4.5"));
    }
}
ZTH_TEST("robust", "for_each_arg: a function with side effects sees the arguments in ORDER") {
    Recorder rec;
    each_or_fail(rec, 1, 2, 3, 4, 5);
    S joined;
    for (const auto& s : rec.seen) joined += s.substr(2);
    ZTH_EXPECT_EQ(joined, S("12345"));
}

// ---- Fast: nothing is copied that does not have to be ----------------------------------------------------
ZTH_TEST("fast", "count_of, all_equal and interpolate take their arguments by reference: no copies, no moves") {
    zth::Tracked a(1), b(1);
    zth::Tracked::reset();
    count_test(2, a, b);
    all_equal_test(true, a, b);
    format_test("{}+{}", "1+1", a, b);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "make_owned from an rvalue moves once and never copies") {
    zth::Tracked::reset();
    auto h = make_or_fail<Holder>(zth::Tracked(7));
    if (h) ZTH_EXPECT_EQ(h->t.value, 7);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 1);  // into the member: the argument itself is passed on by reference
}
ZTH_TEST("fast", "make_owned from an lvalue copies once and never moves") {
    zth::Tracked source(8);
    zth::Tracked::reset();
    auto h = make_or_fail<Holder>(source);
    if (h) ZTH_EXPECT_EQ(h->t.value, 8);
    ZTH_EXPECT_EQ(source.value, 8);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 1);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "for_each_arg passes every argument by reference: no copies, no moves") {
    zth::Tracked a(1);
    Recorder rec;
    zth::Tracked::reset();
    each_or_fail(rec, a, zth::Tracked(2), std::move(a));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(rec.seen.size(), std::size_t{3});
}
