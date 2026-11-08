#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "generic.hpp"
#include "zth_test.hpp"

// A user-defined `<` for the harness's counting type, in ITS namespace so that argument-dependent lookup finds it.
namespace zth {
inline bool operator<(const Tracked& a, const Tracked& b) { return a.value < b.value; }
}  // namespace zth

namespace lib {
struct Widget {
    int id;
};
inline int widget_swaps = 0;
// a swap written for the type itself, in its namespace: argument-dependent lookup finds it
inline void swap(Widget& a, Widget& b) noexcept {
    ++widget_swaps;
    std::swap(a.id, b.id);
}
}  // namespace lib

namespace {

struct Plain {  // no swap of its own: std::swap is the fallback
    int id;
};
struct OnlyLess {  // only operator<: no >, no ==
    int v;
    bool operator<(const OnlyLess& o) const { return v < o.v; }
};
struct Rank {  // `<` looks at the key only: two Ranks with the same key are "equal"
    int key;
    int tag;
    bool operator<(const Rank& o) const { return key < o.key; }
};

// ---- can the function be called like this? (false, not a hard compile error in this hidden file, when it cannot) ---------
template <class T> constexpr bool can_clamp = requires(T v) { clamp_value(v, v, v); };
template <class A, class B> constexpr bool can_min = requires(A a, B b) { min_of(a, b); };
template <class T> constexpr bool can_min_list = requires(T a, T b) { min_of({a, b}); };
template <class T> constexpr bool can_swap = requires(T& a, T& b) { swap_values(a, b); };

template <class T>
void expect_clamp(const char* what, T v, T lo, T hi, T expected) {
    if constexpr (!can_clamp<T>) {
        ::zth::fail(std::string("clamp_value does not compile for ") + what + ": make it a template over the type of the values");
    } else {
        ZTH_EXPECT_TRUE(clamp_value(v, lo, hi) == expected);
    }
}

template <class A, class B>
void expect_min(const char* what, A a, B b, std::common_type_t<A, B> expected) {
    if constexpr (!can_min<A, B>) {
        ::zth::fail(std::string("min_of does not compile for ") + what + ": it needs two type parameters A and B, and the result type std::common_type_t<A, B>");
    } else {
        ZTH_EXPECT_TRUE(min_of(a, b) == expected);
    }
}

template <class T>
void expect_min_list(const char* what, T a, T b, T c, T expected) {
    if constexpr (!can_min_list<T>) {
        ::zth::fail(std::string("min_of({...}) does not compile for ") + what + ": add an overload taking std::initializer_list<T>");
    } else {
        ZTH_EXPECT_TRUE(min_of({a, b, c}) == expected);
    }
}

template <class T>
void expect_swapped(const char* what, T a, T b) {
    if constexpr (!can_swap<T>) {
        ::zth::fail(std::string("swap_values does not compile for ") + what + ": make it a template over the type");
    } else {
        T x = a, y = b;
        swap_values(x, y);
        ZTH_EXPECT_TRUE(x == b && y == a);
    }
}

template <class T>
void clamp_returns_a_reference_test() {
    if constexpr (!can_clamp<T>) {
        ::zth::fail("clamp_value does not compile");
    } else if constexpr (!std::is_reference_v<decltype(clamp_value(std::declval<T&>(), std::declval<T&>(), std::declval<T&>()))>) {
        ::zth::fail("clamp_value returns a copy: it must return a reference to one of its arguments (const T&)");
    } else {
        T v = 5, lo = 0, hi = 3, mid = 2;
        ZTH_EXPECT_TRUE(&clamp_value(v, lo, hi) == &hi);     // too big: the result IS the `high` argument
        ZTH_EXPECT_TRUE(&clamp_value(lo, mid, hi) == &mid);  // too small: the `low` argument
        ZTH_EXPECT_TRUE(&clamp_value(mid, lo, hi) == &mid);  // inside: the value itself
    }
}

template <class T>
void clamp_only_less_test() {
    if constexpr (!can_clamp<T>) {
        ::zth::fail("clamp_value does not compile for a type with only operator<");
    } else {
        T v{9}, lo{1}, hi{5};
        ZTH_EXPECT_EQ(clamp_value(v, lo, hi).v, 5);
        ZTH_EXPECT_EQ(clamp_value(T{0}, lo, hi).v, 1);
        ZTH_EXPECT_EQ(clamp_value(T{3}, lo, hi).v, 3);
    }
}

template <class T>
void min_only_less_test() {
    if constexpr (!can_min<T, T>) {
        ::zth::fail("min_of does not compile for a type with only operator<");
    } else {
        ZTH_EXPECT_EQ(min_of(T{4}, T{2}).v, 2);
        ZTH_EXPECT_EQ(min_of(T{2}, T{4}).v, 2);
    }
}

template <class T>
void min_first_of_equals_test() {
    if constexpr (!can_min<T, T>) {
        ::zth::fail("min_of does not compile");
    } else {
        ZTH_EXPECT_EQ(min_of(T{1, 10}, T{1, 20}).tag, 10);  // equal keys: the FIRST argument
        ZTH_EXPECT_EQ(min_of(T{0, 10}, T{1, 20}).tag, 10);
        ZTH_EXPECT_EQ(min_of(T{1, 10}, T{0, 20}).tag, 20);
    }
}

template <class I, class D, class L, class S>
void mixed_types_test() {
    if constexpr (!can_min<I, D>) {
        ::zth::fail("min_of(int, double) does not compile: the two arguments need independent type parameters A and B");
    } else {
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(min_of(I{1}, D{2.5})), D>));
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(min_of(I{1}, L{2})), L>));
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(min_of(S{1}, I{2})), I>));
        ZTH_EXPECT_TRUE(min_of(I{1}, D{2.5}) == D{1.0});
        ZTH_EXPECT_TRUE(min_of(I{3}, D{2.5}) == D{2.5});  // an int version would truncate 2.5 to 2
        ZTH_EXPECT_TRUE(min_of(D{2.5}, I{3}) == D{2.5});
    }
}

template <class T>
void list_edge_test() {
    if constexpr (!can_min_list<T>) {
        ::zth::fail("min_of({...}) does not compile");
    } else {
        ZTH_EXPECT_EQ(min_of({T{42}}), T{42});
        ZTH_EXPECT_EQ(min_of({T{7}, T{7}, T{7}}), T{7});
        ZTH_EXPECT_EQ(min_of({T{9}, T{8}, T{7}, T{6}, T{5}, T{4}, T{3}, T{2}, T{1}, T{0}}), T{0});
    }
}

template <class T>
void swap_move_only_test() {
    if constexpr (!can_swap<T>) {
        ::zth::fail("swap_values does not compile for std::unique_ptr: it must MOVE, not copy");
    } else {
        T a = std::make_unique<int>(1), b = std::make_unique<int>(2);
        const int* pa = a.get();
        const int* pb = b.get();
        swap_values(a, b);
        ZTH_EXPECT_TRUE(a.get() == pb && b.get() == pa);
        ZTH_EXPECT_EQ(*a, 2);
        ZTH_EXPECT_EQ(*b, 1);
    }
}

template <class W>
void adl_swap_test() {
    if constexpr (!can_swap<W>) {
        ::zth::fail("swap_values does not compile for lib::Widget");
    } else {
        lib::widget_swaps = 0;
        W a{1}, b{2};
        swap_values(a, b);
        ZTH_EXPECT_EQ(a.id, 2);
        ZTH_EXPECT_EQ(b.id, 1);
        // the type's OWN swap must have been called: `using std::swap; swap(a, b);` finds it, `std::swap(a, b)` does not
        ZTH_EXPECT_EQ(lib::widget_swaps, 1);
    }
}

template <class P>
void plain_swap_test() {
    if constexpr (!can_swap<P>) {
        ::zth::fail("swap_values does not compile for a type without a swap of its own");
    } else {
        P a{1}, b{2};
        swap_values(a, b);
        ZTH_EXPECT_EQ(a.id, 2);
        ZTH_EXPECT_EQ(b.id, 1);
    }
}

template <class T>
void tracked_swap_test() {
    if constexpr (!can_swap<T>) {
        ::zth::fail("swap_values does not compile");
    } else {
        T a{1}, b{2};
        T::reset();
        swap_values(a, b);
        ZTH_EXPECT_EQ(a.value, 2);
        ZTH_EXPECT_EQ(b.value, 1);
        ZTH_EXPECT_EQ(T::copies, 0);       // moved, not copied
        ZTH_EXPECT_EQ(T::moves, 1);        // one move construction (the temporary) ...
        ZTH_EXPECT_EQ(T::move_assigns, 2);  // ... and two move assignments
    }
}

template <class T>
void tracked_clamp_test() {
    if constexpr (!can_clamp<T>) {
        ::zth::fail("clamp_value does not compile");
    } else {
        T v{9}, lo{1}, hi{5};
        T::reset();
        (void)clamp_value(v, lo, hi);
        (void)clamp_value(lo, lo, hi);
        ZTH_EXPECT_EQ(T::copies, 0);
        ZTH_EXPECT_EQ(T::moves, 0);
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "clamp_value forces an int into a range") {
    expect_clamp<int>("int", 15, 0, 10, 10);
    expect_clamp<int>("int", -3, 0, 10, 0);
    expect_clamp<int>("int", 7, 0, 10, 7);
    expect_clamp<int>("int", 0, 0, 10, 0);
    expect_clamp<int>("int", 10, 0, 10, 10);
}
ZTH_TEST("works", "clamp_value works for double, char and std::string too") {
    expect_clamp<double>("double", 1.5, 0.0, 1.0, 1.0);
    expect_clamp<double>("double", -0.25, 0.0, 1.0, 0.0);
    expect_clamp<char>("char", 'z', 'a', 'f', 'f');
    expect_clamp<std::string>("std::string", "m", "a", "f", "f");
    expect_clamp<std::string>("std::string", "c", "a", "f", "c");
}
ZTH_TEST("works", "min_of picks the smaller of two values of the same type") {
    expect_min<int, int>("int, int", 3, 7, 3);
    expect_min<int, int>("int, int", 7, 3, 3);
    expect_min<double, double>("double, double", 2.5, -1.5, -1.5);
    expect_min<std::string, std::string>("string, string", "pear", "apple", "apple");
}
ZTH_TEST("works", "min_of takes two DIFFERENT types and returns their common type: min_of(3, 2.5) is 2.5") { mixed_types_test<int, double, long, short>(); }
ZTH_TEST("works", "min_of({...}) finds the smallest of a braced list") {
    expect_min_list<int>("int", 3, 1, 2, 1);
    expect_min_list<int>("int", -5, 0, 5, -5);
    expect_min_list<double>("double", 2.5, 0.5, 1.5, 0.5);
    expect_min_list<std::string>("std::string", "pear", "fig", "kiwi", "fig");
}
ZTH_TEST("works", "swap_values exchanges two ints, two strings and two vectors") {
    expect_swapped<int>("int", 1, 2);
    expect_swapped<std::string>("std::string", "left", "right");
    expect_swapped<std::vector<int>>("std::vector<int>", {1, 2, 3}, {9});
}
ZTH_TEST("works", "swap_values works for move-only types (std::unique_ptr)") { swap_move_only_test<std::unique_ptr<int>>(); }
ZTH_TEST("works", "swap_values calls a type's OWN swap when it has one (argument-dependent lookup), and std::swap otherwise") {
    adl_swap_test<lib::Widget>();
    plain_swap_test<Plain>();
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "clamp_value returns a REFERENCE to one of its arguments, not a copy") { clamp_returns_a_reference_test<int>(); }
ZTH_TEST("robust", "clamp_value needs only operator< from the type") { clamp_only_less_test<OnlyLess>(); }
ZTH_TEST("robust", "min_of needs only operator< from the type") { min_only_less_test<OnlyLess>(); }
ZTH_TEST("robust", "min_of returns the FIRST argument when they are equal") { min_first_of_equals_test<Rank>(); }
ZTH_TEST("robust", "clamp_value with low == high, and with negative ranges") {
    expect_clamp<int>("int", 5, 3, 3, 3);
    expect_clamp<int>("int", 1, 3, 3, 3);
    expect_clamp<int>("int", -7, -10, -2, -7);
    expect_clamp<int>("int", -20, -10, -2, -10);
    expect_clamp<int>("int", 0, -10, -2, -2);
}
ZTH_TEST("robust", "min_of({...}) with a single element and with equal elements") { list_edge_test<int>(); }
ZTH_TEST("robust", "swap_values on large vectors and long strings") {
    expect_swapped<std::vector<int>>("std::vector<int>", std::vector<int>(10000, 1), std::vector<int>(3, 2));
    expect_swapped<std::string>("std::string", std::string(5000, 'a'), std::string(10, 'b'));
}

// ---- Fast ----------------------------------------------------------------------------------------
ZTH_TEST("fast", "swap_values moves its arguments and never copies them") { tracked_swap_test<zth::Tracked>(); }
ZTH_TEST("fast", "clamp_value neither copies nor moves its arguments") { tracked_clamp_test<zth::Tracked>(); }
