#include <cstddef>
#include <deque>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include "constraints.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

struct Opaque {  // no operator<<
    int v;
};

// ---- can this be called / named? (false, not a hard compile error in this hidden file, when the constraints reject it) -----
template <class V> constexpr bool can_mean = requires(const V& v) { mean(v); };
template <class A, class B> constexpr bool can_larger = requires(A a, B b) { larger(a, b); };
template <class T> constexpr bool can_describe = requires(const T& t) { describe(t); };
template <class T> constexpr bool can_accumulate = requires { typename Accumulator<T>; };
template <class V> constexpr bool can_join = requires(const V& v) { join(v, S(", ")); };

template <class V>
void mean_test(const char* what, V v, double expected) {
    if constexpr (!can_mean<V>) {
        ::zth::fail(std::string("mean does not accept ") + what);
    } else {
        ZTH_EXPECT_NEAR(mean(v), expected, 1e-9);
    }
}

template <class A, class B>
void larger_test(const char* what, A a, B b, std::common_type_t<A, B> expected) {
    if constexpr (!can_larger<A, B>) {
        ::zth::fail(std::string("larger does not accept ") + what);
    } else {
        ZTH_EXPECT_TRUE(larger(a, b) == expected);
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(larger(a, b)), std::common_type_t<A, B>>));
    }
}

// what describe(x) says for a value of type T. An ambiguous call (neither overload more constrained) is reported, not a compile error here.
template <class T>
void describe_test(const char* what, T value, const char* expected) {
    if constexpr (!can_describe<T>) {
        ::zth::fail(std::string("describe(") + what +
                    ") does not compile (ambiguous?): define Number IN TERMS OF Integer, so that the Integer overload is more constrained than the Number overload");
    } else {
        ZTH_EXPECT_EQ(describe(value), S(expected));
    }
}

template <class T>
void accumulator_test(const char* what, T a, T b, T expected_total, double expected_mean) {
    if constexpr (!can_accumulate<T>) {
        ::zth::fail(std::string("Accumulator<") + what + "> does not compile");
    } else {
        Accumulator<T> acc;
        ZTH_EXPECT_EQ(acc.count(), std::size_t{0});
        ZTH_EXPECT_NEAR(acc.mean(), 0.0, 1e-12);
        acc.add(a);
        acc.add(b);
        ZTH_EXPECT_EQ(acc.count(), std::size_t{2});
        ZTH_EXPECT_TRUE(acc.total() == expected_total);
        ZTH_EXPECT_NEAR(acc.mean(), expected_mean, 1e-9);
    }
}

template <class V>
void join_test(const char* what, V v, const char* separator, const char* expected) {
    if constexpr (!can_join<V>) {
        ::zth::fail(std::string("join does not accept ") + what);
    } else {
        ZTH_EXPECT_EQ(join(v, S(separator)), S(expected));
    }
}

}  // namespace

// ---- Works: the concepts ---------------------------------------------------------------------------------
ZTH_TEST("works", "Integer: the integral types except bool") {
    ZTH_EXPECT_TRUE(Integer<int>);
    ZTH_EXPECT_TRUE(Integer<long long>);
    ZTH_EXPECT_TRUE(Integer<unsigned char>);
    ZTH_EXPECT_TRUE(Integer<char>);
    ZTH_EXPECT_FALSE(Integer<bool>);
    ZTH_EXPECT_FALSE(Integer<double>);
    ZTH_EXPECT_FALSE(Integer<S>);
    ZTH_EXPECT_FALSE(Integer<int*>);
}
ZTH_TEST("works", "Number: an Integer or a floating-point type") {
    ZTH_EXPECT_TRUE(Number<int>);
    ZTH_EXPECT_TRUE(Number<unsigned>);
    ZTH_EXPECT_TRUE(Number<double>);
    ZTH_EXPECT_TRUE(Number<float>);
    ZTH_EXPECT_FALSE(Number<bool>);
    ZTH_EXPECT_FALSE(Number<S>);
    ZTH_EXPECT_FALSE(Number<const char*>);
    ZTH_EXPECT_FALSE(Number<Opaque>);
}
ZTH_TEST("works", "Printable: a type that can be written to a std::ostream") {
    ZTH_EXPECT_TRUE(Printable<int>);
    ZTH_EXPECT_TRUE(Printable<double>);
    ZTH_EXPECT_TRUE(Printable<S>);
    ZTH_EXPECT_TRUE(Printable<const char*>);
    ZTH_EXPECT_FALSE(Printable<Opaque>);
    ZTH_EXPECT_FALSE(Printable<std::vector<int>>);
}
ZTH_TEST("works", "Sequence: something with value_type, begin(), end() and size()") {
    ZTH_EXPECT_TRUE(Sequence<std::vector<int>>);
    ZTH_EXPECT_TRUE(Sequence<std::list<S>>);
    ZTH_EXPECT_TRUE(Sequence<std::deque<double>>);
    ZTH_EXPECT_TRUE(Sequence<S>);
    ZTH_EXPECT_FALSE(Sequence<int>);
    ZTH_EXPECT_FALSE(Sequence<Opaque>);
    ZTH_EXPECT_FALSE(Sequence<int*>);
}

// ---- Works: the constrained templates -------------------------------------------------------------------------
ZTH_TEST("works", "mean of a vector of ints, of doubles, of a list") {
    mean_test("vector<int>", std::vector<int>{1, 2, 3, 4}, 2.5);
    mean_test("vector<double>", std::vector<double>{0.5, 0.25}, 0.375);
    mean_test("list<long>", std::list<long>{10, 20, 60}, 30.0);
    mean_test("vector<unsigned char>", std::vector<unsigned char>{200, 100}, 150.0);
}
ZTH_TEST("works", "mean refuses a sequence of things that are not Numbers (a COMPILE error, checked here with requires)") {
    ZTH_EXPECT_TRUE((can_mean<std::vector<int>>));
    ZTH_EXPECT_FALSE((can_mean<std::vector<S>>));
    ZTH_EXPECT_FALSE((can_mean<std::vector<bool>>));
    ZTH_EXPECT_FALSE((can_mean<std::vector<Opaque>>));
    ZTH_EXPECT_FALSE((can_mean<int>));
}
ZTH_TEST("works", "larger takes two Numbers, of different types if you like, and returns their common type") {
    larger_test<int, int>("int, int", 7, 3, 7);
    larger_test<int, double>("int, double", 1, 2.5, 2.5);
    larger_test<double, int>("double, int", 2.5, 1, 2.5);
    larger_test<double, int>("double, int", 0.5, 3, 3.0);
    larger_test<long, unsigned char>("long, unsigned char", 5L, static_cast<unsigned char>(9), 9L);
}
ZTH_TEST("works", "larger refuses arguments that are not Numbers") {
    ZTH_EXPECT_TRUE((can_larger<int, double>));
    ZTH_EXPECT_FALSE((can_larger<S, S>));
    ZTH_EXPECT_FALSE((can_larger<int, S>));
    ZTH_EXPECT_FALSE((can_larger<bool, bool>));
}
ZTH_TEST("works", "describe: something / a number / an integer, chosen by the most constrained overload") {
    describe_test<S>("std::string", S("x"), "something");
    describe_test<double>("double", 1.5, "a number");
    describe_test<float>("float", 1.5f, "a number");
    describe_test<int>("int", 42, "an integer");
    describe_test<unsigned long>("unsigned long", 7UL, "an integer");
    describe_test<char>("char", 'c', "an integer");
}
ZTH_TEST("works", "describe treats bool and other non-numbers as 'something'") {
    describe_test<bool>("bool", true, "something");
    describe_test<const char*>("const char*", "text", "something");
    describe_test<Opaque>("Opaque", Opaque{1}, "something");
}
ZTH_TEST("works", "Accumulator<int> and Accumulator<double> add up and average") {
    accumulator_test<int>("int", 10, 20, 30, 15.0);
    accumulator_test<double>("double", 0.5, 1.5, 2.0, 1.0);
    accumulator_test<unsigned>("unsigned", 3u, 4u, 7u, 3.5);
}
ZTH_TEST("works", "Accumulator refuses types that are not Numbers") {
    ZTH_EXPECT_TRUE((can_accumulate<int>));
    ZTH_EXPECT_FALSE((can_accumulate<S>));
    ZTH_EXPECT_FALSE((can_accumulate<bool>));
    ZTH_EXPECT_FALSE((can_accumulate<Opaque>));
}
ZTH_TEST("works", "join writes the elements separated by the separator, nothing at the end") {
    join_test("vector<int>", std::vector<int>{1, 2, 3}, ", ", "1, 2, 3");
    join_test("vector<string>", std::vector<S>{"a", "b"}, "-", "a-b");
    join_test("list<double>", std::list<double>{1.5, 2.5}, " | ", "1.5 | 2.5");
    join_test("vector<int>", std::vector<int>{7}, ", ", "7");
    join_test("vector<int>", std::vector<int>{}, ", ", "");
}
ZTH_TEST("works", "join refuses elements that cannot be printed") {
    ZTH_EXPECT_TRUE((can_join<std::vector<int>>));
    ZTH_EXPECT_FALSE((can_join<std::vector<Opaque>>));
    ZTH_EXPECT_FALSE((can_join<int>));
}

// ---- Robust ------------------------------------------------------------------------------------------
ZTH_TEST("robust", "the mean of an empty sequence is 0.0, not NaN") {
    mean_test("empty vector<int>", std::vector<int>{}, 0.0);
    mean_test("empty list<double>", std::list<double>{}, 0.0);
}
ZTH_TEST("robust", "the mean is exact for doubles (not truncated to an integer while adding up)") {
    mean_test("vector<double>", std::vector<double>{0.1, 0.2, 0.3, 0.4}, 0.25);
    mean_test("vector<double>", std::vector<double>{0.4, 0.4}, 0.4);
    mean_test("vector<float>", std::vector<float>{0.5f, 0.25f}, 0.375);
    Accumulator<double> acc;
    acc.add(0.25);
    acc.add(0.25);
    ZTH_EXPECT_NEAR(acc.total(), 0.5, 1e-12);
}
ZTH_TEST("robust", "larger with negative numbers and equal numbers") {
    larger_test<int, int>("int, int", -5, -2, -2);
    larger_test<int, double>("int, double", 3, 3.0, 3.0);
    larger_test<double, double>("double, double", -0.5, -0.25, -0.25);
}
ZTH_TEST("robust", "describe also works for const and reference arguments") {
    const int ci = 5;
    const double cd = 2.0;
    int i = 1;
    int& ri = i;
    describe_test<int>("const int", ci, "an integer");
    describe_test<double>("const double", cd, "a number");
    describe_test<int>("int&", ri, "an integer");
}
ZTH_TEST("robust", "an Accumulator of a floating-point type handles many values, and of an integer type does not lose them") {
    Accumulator<long long> big;
    for (int i = 1; i <= 1000; ++i) big.add(i);
    ZTH_EXPECT_EQ(big.total(), 500500LL);
    ZTH_EXPECT_EQ(big.count(), std::size_t{1000});
    ZTH_EXPECT_NEAR(big.mean(), 500.5, 1e-9);
}
