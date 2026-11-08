#include <array>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include "generic_bugs.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

template <class A, class B> constexpr bool can_largest = requires(A a, B b) { largest(a, b); };
template <class T> constexpr bool can_twice = requires(T x) { twice(x); };

// largest(a, b) must compile, have the common type of its arguments, and be `expected`
template <class A, class B>
void largest_test(A a, B b, std::common_type_t<A, B> expected) {
    if constexpr (!can_largest<A, B>) {
        ::zth::fail("largest(a, b) does not compile for two arguments of DIFFERENT types");
    } else {
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(largest(a, b)), std::common_type_t<A, B>>));
        ZTH_EXPECT_TRUE(largest(a, b) == expected);
    }
}

template <class T>
void twice_test(T x, T expected) {
    if constexpr (!can_twice<T>) {
        ::zth::fail("twice(x) does not compile for this type: is it constrained too narrowly?");
    } else {
        ZTH_EXPECT_TRUE(twice(x) == expected);
    }
}

}  // namespace

// ---- Works ----------------------------------------------------------------------------------------------
ZTH_TEST("works", "largest of two ints") {
    largest_test<int, int>(7, 2, 7);
    largest_test<int, int>(2, 7, 7);
}
ZTH_TEST("works", "largest of an int and a double has the common type (double)") {
    largest_test<int, double>(3, 4.5, 4.5);
    largest_test<double, int>(4.5, 3, 4.5);
    largest_test<int, double>(5, 4.5, 5.0);
}
ZTH_TEST("works", "first_of returns the first element of any container with operator[]") {
    ZTH_EXPECT_EQ(first_of(std::vector<int>{5, 6}), 5);
    ZTH_EXPECT_EQ(first_of(S("hey")), 'h');
    ZTH_EXPECT_EQ(first_of(std::array<double, 2>{1.5, 2.5}), 1.5);
}
ZTH_TEST("works", "twice works for integers") {
    twice_test<int>(21, 42);
    twice_test<long>(5L, 10L);
}
ZTH_TEST("works", "twice works for floating-point numbers too") {
    twice_test<double>(2.5, 5.0);
    twice_test<double>(0.75, 1.5);
    twice_test<float>(1.5f, 3.0f);
}
ZTH_TEST("works", "twice still refuses types that are not numbers (a compile-time check)") {
    ZTH_EXPECT_FALSE(can_twice<S>);
    ZTH_EXPECT_FALSE(can_twice<std::vector<int>>);
    ZTH_EXPECT_TRUE(can_twice<int>);
}
ZTH_TEST("works", "sorted_versions sorts by major, then by minor") {
    const auto v = sorted_versions({{1, 10}, {1, 2}, {0, 9}});
    ZTH_EXPECT_EQ(v.size(), std::size_t{3});
    if (v.size() == 3) {
        ZTH_EXPECT_TRUE(v[0].major == 0 && v[0].minor == 9);
        ZTH_EXPECT_TRUE(v[1].major == 1 && v[1].minor == 2);
        ZTH_EXPECT_TRUE(v[2].major == 1 && v[2].minor == 10);
    }
}
ZTH_TEST("works", "stack_demo pushes 3 and 4 and pops them (last in, first out)") { ZTH_EXPECT_EQ(stack_demo(), 43); }
ZTH_TEST("works", "buffer_demo: a Buffer<4> has a capacity of 4") { ZTH_EXPECT_EQ(buffer_demo(), std::size_t{4}); }

// ---- Robust ---------------------------------------------------------------------------------------------
ZTH_TEST("robust", "largest with negative numbers, equal numbers and other types") {
    largest_test<int, int>(-5, -2, -2);
    largest_test<int, double>(3, 3.0, 3.0);
    largest_test<long, int>(10L, 20, 20L);
    largest_test<unsigned, unsigned>(3u, 9u, 9u);
    largest_test<S, S>(S("apple"), S("pear"), S("pear"));
}
ZTH_TEST("robust", "first_of for a container of strings and a list-like type") {
    ZTH_EXPECT_EQ(first_of(std::vector<S>{"x", "y"}), S("x"));
    const std::vector<int> v{9};
    ZTH_EXPECT_EQ(first_of(v), 9);
}
ZTH_TEST("robust", "twice of zero and of negative numbers") {
    twice_test<int>(0, 0);
    twice_test<int>(-3, -6);
    twice_test<double>(-0.5, -1.0);
}
ZTH_TEST("robust", "sorted_versions: equal versions, an empty list and a single version") {
    const auto same = sorted_versions({{2, 1}, {2, 1}, {1, 5}});
    ZTH_EXPECT_EQ(same.size(), std::size_t{3});
    if (same.size() == 3) {
        ZTH_EXPECT_TRUE(same[0].major == 1);
        ZTH_EXPECT_TRUE(same[1].major == 2 && same[2].major == 2);
    }
    ZTH_EXPECT_TRUE(sorted_versions({}).empty());
    const auto one = sorted_versions({{7, 7}});
    ZTH_EXPECT_EQ(one.size(), std::size_t{1});
}
ZTH_TEST("robust", "a Version with the larger minor but the smaller major still sorts first") {
    const auto v = sorted_versions({{2, 0}, {1, 99}});
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    if (v.size() == 2) {
        ZTH_EXPECT_EQ(v[0].major, 1);
        ZTH_EXPECT_EQ(v[1].major, 2);
    }
}
ZTH_TEST("robust", "Stack<T> keeps the order and the size") {
    Stack<S> s;
    s.push("a");
    s.push("b");
    s.push("c");
    ZTH_EXPECT_EQ(s.size(), std::size_t{3});
    ZTH_EXPECT_EQ(s.pop(), S("c"));
    ZTH_EXPECT_EQ(s.pop(), S("b"));
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
}
ZTH_TEST("robust", "Buffer<N>::capacity() is N, for any N, at compile time") {
    ZTH_EXPECT_EQ(Buffer<1>::capacity(), std::size_t{1});
    ZTH_EXPECT_EQ(Buffer<16>::capacity(), std::size_t{16});
    ZTH_EXPECT_EQ(sizeof(Buffer<8>), 8 * sizeof(int));
}
