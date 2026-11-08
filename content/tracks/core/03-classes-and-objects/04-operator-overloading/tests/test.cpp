#include <concepts>
#include <cstddef>
#include <cstring>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "vec2.hpp"
#include "zth_test.hpp"

namespace {

using std::size_t;

// ---- API shape, as templates: a missing or wrongly-shaped operator is a clear test failure, not a compile error here.
template <class V> constexpr bool has_plus = requires(const V& a, const V& b) { { a + b } -> std::same_as<V>; };
template <class V> constexpr bool has_minus = requires(const V& a, const V& b) { { a - b } -> std::same_as<V>; };
template <class V> constexpr bool has_times_right = requires(const V& a) { { a * 2 } -> std::same_as<V>; };
template <class V> constexpr bool has_times_left = requires(const V& a) { { 2 * a } -> std::same_as<V>; };
template <class V> constexpr bool has_negate = requires(const V& a) { { -a } -> std::same_as<V>; };
template <class V> constexpr bool has_eq = requires(const V& a, const V& b) { { a == b } -> std::convertible_to<bool>; };
template <class V> constexpr bool has_ne = requires(const V& a, const V& b) { { a != b } -> std::convertible_to<bool>; };
template <class V> constexpr bool has_plus_eq = requires(V a, const V& b) { { a += b } -> std::same_as<V&>; };
template <class V> constexpr bool has_minus_eq = requires(V a, const V& b) { { a -= b } -> std::same_as<V&>; };
template <class V> constexpr bool has_times_eq = requires(V a) { { a *= 2 } -> std::same_as<V&>; };
template <class V> constexpr bool has_subscript = requires(V a) { { a[size_t{0}] } -> std::same_as<int&>; };
template <class V> constexpr bool has_const_subscript = requires(const V& a) { { a[size_t{0}] } -> std::convertible_to<int>; };
template <class V> constexpr bool const_subscript_read_only = !requires(const V& a) { a[size_t{0}] = 1; };
template <class V> constexpr bool has_prefix = requires(V a) { { ++a } -> std::same_as<V&>; };
template <class V> constexpr bool has_postfix = requires(V a) { { a++ } -> std::same_as<V>; };
template <class V> constexpr bool has_bool = requires(const V& a) { static_cast<bool>(a); };
template <class V> constexpr bool bool_is_explicit = !std::is_convertible_v<V, bool>;
template <class V> constexpr bool has_dot = requires(const V& a, const V& b) { { dot(a, b) } -> std::convertible_to<int>; };
template <class V> constexpr bool has_print = requires(std::ostream& os, const V& v) { { os << v } -> std::same_as<std::ostream&>; };

#define NEED(shape, what, ...)                 \
    if constexpr (!(shape)) {                  \
        ::zth::fail(std::string(what));        \
    } else {                                   \
        __VA_ARGS__                            \
    }

template <class V>
void plus_minus_test() {
    NEED(has_plus<V>&& has_minus<V>, "`a + b` and `a - b` must work on const vectors and return a new Vec2 (non-member operators taking the left side by value, built from += and -=)", {
        const V a(1, 2), b(10, 20);
        ZTH_EXPECT_EQ(a + b, V(11, 22));
        ZTH_EXPECT_EQ(b - a, V(9, 18));
        ZTH_EXPECT_EQ(a - b, V(-9, -18));
        ZTH_EXPECT_EQ(a, V(1, 2));  // neither operand was changed
        ZTH_EXPECT_EQ(b, V(10, 20));
        ZTH_EXPECT_EQ(a + b + a, V(12, 24));
        ZTH_EXPECT_EQ(a + (b - a), b);
    })
}

template <class V>
void scalar_test() {
    NEED(has_times_right<V>&& has_times_left<V>, "both `v * 3` and `3 * v` must work (two non-member operators) and return a Vec2", {
        const V v(3, -4);
        ZTH_EXPECT_EQ(v * 3, V(9, -12));
        ZTH_EXPECT_EQ(3 * v, V(9, -12));
        ZTH_EXPECT_EQ(-2 * v, V(-6, 8));
        ZTH_EXPECT_EQ(v * 0, V(0, 0));
        ZTH_EXPECT_EQ(v, V(3, -4));
    })
}

template <class V>
void negate_test() {
    NEED(has_negate<V>, "unary minus `-v` on a const Vec2, returning a Vec2", {
        const V v(3, -4);
        ZTH_EXPECT_EQ(-v, V(-3, 4));
        ZTH_EXPECT_EQ(-(-v), v);
        ZTH_EXPECT_EQ(-V(), V());
        ZTH_EXPECT_EQ(v, V(3, -4));
    })
}

template <class V>
void equality_test() {
    NEED(has_eq<V>&& has_ne<V>, "`==` and `!=` on const vectors (a defaulted `friend bool operator==` gives you both)", {
        const V a(1, 2);
        ZTH_EXPECT_TRUE(a == V(1, 2));
        ZTH_EXPECT_FALSE(a == V(1, 3));
        ZTH_EXPECT_FALSE(a == V(2, 2));
        ZTH_EXPECT_FALSE(a == V(2, 1));
        ZTH_EXPECT_TRUE(a != V(1, 3));
        ZTH_EXPECT_FALSE(a != V(1, 2));
    })
}

template <class V>
void compound_test() {
    NEED(has_plus_eq<V>&& has_minus_eq<V>&& has_times_eq<V>, "`+=`, `-=` and `*=` must be members that return the object itself by reference (`Vec2&`)", {
        V a(1, 2);
        const V b(10, 20);
        V& r = (a += b);
        ZTH_EXPECT_TRUE(&r == &a);
        ZTH_EXPECT_EQ(a, V(11, 22));
        (a += b) += b;  // chaining only works if += returns a reference to a itself
        ZTH_EXPECT_EQ(a, V(31, 62));
        V& r2 = (a -= b);
        ZTH_EXPECT_TRUE(&r2 == &a);
        ZTH_EXPECT_EQ(a, V(21, 42));
        V& r3 = (a *= 2);
        ZTH_EXPECT_TRUE(&r3 == &a);
        ZTH_EXPECT_EQ(a, V(42, 84));
        a *= -1;
        ZTH_EXPECT_EQ(a, V(-42, -84));
    })
}

template <class V>
void alias_test() {
    NEED(has_plus_eq<V>&& has_minus_eq<V>&& has_times_eq<V>, "`+=`, `-=` and `*=` must exist and return `Vec2&`", {
        V a(3, -5);
        a += a;
        ZTH_EXPECT_EQ(a, V(6, -10));
        a -= a;
        ZTH_EXPECT_EQ(a, V(0, 0));
        V b(2, 7);
        b = b - b;
        ZTH_EXPECT_EQ(b, V(0, 0));
        V c(2, 7);
        c = c + c;
        ZTH_EXPECT_EQ(c, V(4, 14));
    })
}

template <class V>
void subscript_test() {
    NEED(has_subscript<V>&& has_const_subscript<V>&& const_subscript_read_only<V>,
         "`v[i]` needs two overloads: one returning `int&` (so `v[0] = 5` works) and a const one returning a const reference or a copy", {
        V v(7, 8);
        ZTH_EXPECT_EQ(v[0], 7);
        ZTH_EXPECT_EQ(v[1], 8);
        v[0] = 100;
        v[1] += 5;
        ZTH_EXPECT_EQ(v, V(100, 13));
        const V c(-1, -2);
        ZTH_EXPECT_EQ(c[0], -1);
        ZTH_EXPECT_EQ(c[1], -2);
    })
}

template <class V>
void subscript_bounds_test() {
    NEED(has_subscript<V>&& has_const_subscript<V>, "`v[i]` needs a const and a non-const overload", {
        V v(7, 8);
        const V c(7, 8);
        ZTH_EXPECT_THROWS(std::out_of_range, (void)v[2]);
        ZTH_EXPECT_THROWS(std::out_of_range, (void)v[100]);
        ZTH_EXPECT_THROWS(std::out_of_range, (void)c[2]);
        ZTH_EXPECT_THROWS(std::out_of_range, (void)c[static_cast<size_t>(-1)]);
        ZTH_EXPECT_NO_THROW((void)v[1]);
        ZTH_EXPECT_NO_THROW((void)c[0]);
    })
}

template <class V>
void bool_test() {
    NEED(has_bool<V>, "`explicit operator bool() const`: true for any vector other than (0, 0)", {
        ZTH_EXPECT_TRUE(bool_is_explicit<V>);
        const V z{}, x(1, 0), y(0, 1), n(-3, -3);
        ZTH_EXPECT_FALSE(static_cast<bool>(z));
        ZTH_EXPECT_TRUE(static_cast<bool>(x));
        ZTH_EXPECT_TRUE(static_cast<bool>(y));
        ZTH_EXPECT_TRUE(static_cast<bool>(n));
        int hits = 0;
        if (x) ++hits;
        if (z) ++hits;
        if (!z) ++hits;
        ZTH_EXPECT_EQ(hits, 2);
    })
}

template <class V>
void explicit_bool_test() {
    NEED(has_bool<V>, "`explicit operator bool() const`", {
        ZTH_EXPECT_TRUE(bool_is_explicit<V>);  // not implicit: `int n = v;` and `v + 1` must not compile
    })
}

template <class V>
void increment_test() {
    NEED(has_prefix<V>&& has_postfix<V>, "`++v` must return `Vec2&` and `v++` (the version with a dummy `int` parameter) must return a `Vec2` by value", {
        V a(1, 2);
        V old = a++;
        ZTH_EXPECT_EQ(old, V(1, 2));  // postfix returns the value from BEFORE the change
        ZTH_EXPECT_EQ(a, V(2, 3));
        V& r = ++a;
        ZTH_EXPECT_TRUE(&r == &a);
        ZTH_EXPECT_EQ(a, V(3, 4));
        ++(++a);
        ZTH_EXPECT_EQ(a, V(5, 6));
        V b = ++a;
        ZTH_EXPECT_EQ(b, V(6, 7));
    })
}

template <class V>
void dot_test() {
    NEED(has_dot<V>, "`int dot(const Vec2&, const Vec2&)`", {
        const V a(1, 2), b(3, 4);
        ZTH_EXPECT_EQ(dot(a, b), 11);
        ZTH_EXPECT_EQ(dot(V(1, 0), V(0, 1)), 0);
        ZTH_EXPECT_EQ(dot(a, a), 5);
        ZTH_EXPECT_EQ(dot(V(-2, 3), V(5, 7)), 11);
    })
}

template <class V>
void print_test() {
    NEED(has_print<V>, "`std::ostream& operator<<(std::ostream&, const Vec2&)`: take the stream by reference and RETURN it", {
        std::ostringstream os;
        os << V(3, -4);
        ZTH_EXPECT_EQ(os.str(), std::string("(3, -4)"));
        std::ostringstream chain;
        chain << "a=" << V(1, 2) << " b=" << V(0, 0) << '!';
        ZTH_EXPECT_EQ(chain.str(), std::string("a=(1, 2) b=(0, 0)!"));
    })
}

template <class V>
void const_everything_test() {
    NEED(has_plus<V>&& has_minus<V>&& has_times_right<V>&& has_times_left<V>&& has_negate<V>&& has_eq<V>&& has_const_subscript<V>&& has_dot<V>&& has_print<V>&& has_bool<V>,
         "every read-only operation must work on const vectors (mark member operators const; take parameters by const reference)", {
        const V a(1, 2), b(3, 4);
        std::ostringstream os;
        os << (a + b) << (a - b) << (a * 2) << (2 * a) << (-a) << dot(a, b) << (a == b) << a[1] << static_cast<bool>(a);
        ZTH_EXPECT_EQ(os.str(), std::string("(4, 6)(-2, -2)(2, 4)(2, 4)(-1, -2)11021"));
    })
}

template <class V>
void property_test() {
    NEED(has_plus<V>&& has_minus<V>&& has_times_right<V>&& has_times_left<V>&& has_negate<V>&& has_eq<V>&& has_plus_eq<V>&& has_dot<V>,
         "+, -, *, unary -, ==, += and dot must all exist", {
        zth::Lcg rng(99);
        auto next = [&] { return static_cast<int>(rng.next() % 201) - 100; };
        for (int i = 0; i < 300; ++i) {
            const V a(next(), next()), b(next(), next()), c(next(), next());
            const int k = next();
            ZTH_EXPECT_EQ(a + b, b + a);
            ZTH_EXPECT_EQ((a + b) + c, a + (b + c));
            ZTH_EXPECT_EQ((a + b) - b, a);
            ZTH_EXPECT_EQ(a - b, a + (-b));
            ZTH_EXPECT_EQ(-(-a), a);
            ZTH_EXPECT_EQ(k * a, a * k);
            ZTH_EXPECT_EQ(a * (k + 1), a * k + a);
            ZTH_EXPECT_EQ(dot(a, b), dot(b, a));
            ZTH_EXPECT_EQ(dot(a + b, c), dot(a, c) + dot(b, c));
            V s = a;
            s += b;
            ZTH_EXPECT_EQ(s, a + b);
        }
    })
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a Vec2 holds x and y (a default one is (0, 0))") {
    const Vec2 a(3, -4);
    ZTH_EXPECT_EQ(a.x(), 3);
    ZTH_EXPECT_EQ(a.y(), -4);
    const Vec2 z{};
    ZTH_EXPECT_EQ(z.x(), 0);
    ZTH_EXPECT_EQ(z.y(), 0);
}
ZTH_TEST("works", "a + b and a - b make a new vector and change neither operand") { plus_minus_test<Vec2>(); }
ZTH_TEST("works", "v * 3 and 3 * v both scale the vector") { scalar_test<Vec2>(); }
ZTH_TEST("works", "unary minus negates both parts without changing the vector") { negate_test<Vec2>(); }
ZTH_TEST("works", "== and != compare both parts") { equality_test<Vec2>(); }
ZTH_TEST("works", "+=, -= and *= change the vector and return it (so they chain)") { compound_test<Vec2>(); }
ZTH_TEST("works", "v[0] is x and v[1] is y, for reading and (on a non-const vector) writing") { subscript_test<Vec2>(); }
ZTH_TEST("works", "dot(a, b) is the dot product") { dot_test<Vec2>(); }
ZTH_TEST("works", "operator<< prints (x, y) and can be chained") { print_test<Vec2>(); }

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "v[i] with an index other than 0 or 1 throws std::out_of_range (const and non-const)") { subscript_bounds_test<Vec2>(); }
ZTH_TEST("robust", "a default-constructed Vec2 is (0, 0) even when the memory held garbage before") {
    alignas(Vec2) unsigned char buf[sizeof(Vec2)];
    std::memset(buf, 0xAB, sizeof buf);
    Vec2* v = new (buf) Vec2;  // default-initialization: only the class itself can give x_ and y_ a value
    const int x = v->x(), y = v->y();
    v->~Vec2();
    ZTH_EXPECT_EQ(x, 0);
    ZTH_EXPECT_EQ(y, 0);
}
ZTH_TEST("robust", "explicit operator bool: true unless (0, 0), and not usable as an implicit conversion") { bool_test<Vec2>(); }
ZTH_TEST("robust", "operator bool is explicit (no accidental `int n = v`)") { explicit_bool_test<Vec2>(); }
ZTH_TEST("robust", "++v returns the changed vector itself, v++ returns a copy of the OLD value") { increment_test<Vec2>(); }
ZTH_TEST("robust", "a += a, a -= a and a = a + a still work when both sides are the same object") { alias_test<Vec2>(); }
ZTH_TEST("robust", "every read-only operator works on const vectors") { const_everything_test<Vec2>(); }
ZTH_TEST("robust", "algebraic properties hold for many random vectors") { property_test<Vec2>(); }
