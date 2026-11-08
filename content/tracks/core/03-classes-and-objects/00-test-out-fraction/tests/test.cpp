#include <algorithm>
#include <concepts>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "fraction.hpp"
#include "zth_test.hpp"

namespace {

std::string str(const Fraction& f) {
    std::ostringstream os;
    os << f;
    return os.str();
}

// true if `f()` throws exactly an Ex (or something derived from it); any other outcome is false
// API-shape checks as templates: if the learner's class does not have the shape, these are false instead of a
// compile error inside this hidden file.
template <class F>
constexpr bool int_on_either_side = requires(F f) {
    f + 2; 2 + f; f - 2; 2 - f; f * 2; 2 * f; f / 2; 2 / f;
};
template <class F>
constexpr bool has_all_comparisons = requires(F a, F b) {
    a == b; a != b; a < b; a <= b; a > b; a >= b;
};

// The checks that mix integers and fractions live in a template, so they are only compiled when
// int_on_either_side<Fraction> says they are valid (an ambiguous or missing operator would otherwise be a compile
// error in this hidden file, which the learner cannot see).
template <class F>
void check_mixed() {
    ZTH_EXPECT_EQ(F(1, 2) + 2, F(5, 2));
    ZTH_EXPECT_EQ(2 + F(1, 2), F(5, 2));
    ZTH_EXPECT_EQ(1 - F(1, 3), F(2, 3));
    ZTH_EXPECT_EQ(F(1, 3) - 1, F(-2, 3));
    ZTH_EXPECT_EQ(3 * F(1, 3), F(1));
    ZTH_EXPECT_EQ(2 / F(3, 4), F(8, 3));
    ZTH_EXPECT_EQ(F(3, 4) / 3, F(1, 4));
}

template <class F>
void mixed_test() {
    if constexpr (int_on_either_side<F>) {  // dependent condition: the other branch is not instantiated
        check_mixed<F>();
    } else {
        ::zth::fail("`2 + f` or `f + 2` (or - * /) does not compile, or is ambiguous: the binary operators must be non-member functions (or friends) so that both operands can be converted, and Fraction -> double must be explicit");
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "6/8 is reduced to 3/4") {
    const Fraction f(6, 8);
    ZTH_EXPECT_EQ(f.numerator(), 3LL);
    ZTH_EXPECT_EQ(f.denominator(), 4LL);
}
ZTH_TEST("works", "the denominator is never negative: 1/-2 is -1/2, and -1/-2 is 1/2") {
    const Fraction a(1, -2), b(-1, -2);
    ZTH_EXPECT_EQ(a.numerator(), -1LL);
    ZTH_EXPECT_EQ(a.denominator(), 2LL);
    ZTH_EXPECT_EQ(b.numerator(), 1LL);
    ZTH_EXPECT_EQ(b.denominator(), 2LL);
}
ZTH_TEST("works", "zero is 0/1, however it is written; a whole number has denominator 1") {
    for (const Fraction& z : {Fraction(), Fraction(0), Fraction(0, 5), Fraction(0, -7)}) {
        ZTH_EXPECT_EQ(z.numerator(), 0LL);
        ZTH_EXPECT_EQ(z.denominator(), 1LL);
    }
    ZTH_EXPECT_EQ(Fraction(3).denominator(), 1LL);
    ZTH_EXPECT_EQ(Fraction(-3).numerator(), -3LL);
}
ZTH_TEST("works", "1/2 + 1/3 is 5/6") { ZTH_EXPECT_EQ(Fraction(1, 2) + Fraction(1, 3), Fraction(5, 6)); }
ZTH_TEST("works", "sums are reduced: 1/2 + 1/2 is 1, not 2/2") {
    const Fraction s = Fraction(1, 2) + Fraction(1, 2);
    ZTH_EXPECT_EQ(s.numerator(), 1LL);
    ZTH_EXPECT_EQ(s.denominator(), 1LL);
}
ZTH_TEST("works", "1/2 - 1/3 is 1/6, and 3/4 - 1/4 is 1/2") {
    ZTH_EXPECT_EQ(Fraction(1, 2) - Fraction(1, 3), Fraction(1, 6));
    ZTH_EXPECT_EQ(Fraction(3, 4) - Fraction(1, 4), Fraction(1, 2));
}
ZTH_TEST("works", "2/3 * 3/4 is 1/2") { ZTH_EXPECT_EQ(Fraction(2, 3) * Fraction(3, 4), Fraction(1, 2)); }
ZTH_TEST("works", "(1/2) / (3/4) is 2/3") { ZTH_EXPECT_EQ(Fraction(1, 2) / Fraction(3, 4), Fraction(2, 3)); }
ZTH_TEST("works", "unary minus negates, and zero stays 0/1") {
    ZTH_EXPECT_EQ(-Fraction(3, 4), Fraction(-3, 4));
    ZTH_EXPECT_EQ(-Fraction(-1, 2), Fraction(1, 2));
    ZTH_EXPECT_EQ((-Fraction(0)).denominator(), 1LL);
}
ZTH_TEST("works", "equal values compare equal however they were written") {
    ZTH_EXPECT_TRUE(Fraction(1, 2) == Fraction(2, 4));
    ZTH_EXPECT_TRUE(Fraction(1, 2) != Fraction(1, 3));
    ZTH_EXPECT_TRUE(Fraction(-1, 2) == Fraction(1, -2));
    ZTH_EXPECT_TRUE(Fraction(0) == Fraction(0, 9));
}
ZTH_TEST("works", "ordering: <, <=, >, >=") {
    ZTH_EXPECT_TRUE(Fraction(1, 3) < Fraction(1, 2));
    ZTH_EXPECT_TRUE(Fraction(1, 2) > Fraction(1, 3));
    ZTH_EXPECT_TRUE(Fraction(1, 2) <= Fraction(2, 4));
    ZTH_EXPECT_TRUE(Fraction(1, 2) >= Fraction(2, 4));
    ZTH_EXPECT_FALSE(Fraction(1, 2) < Fraction(2, 4));
    ZTH_EXPECT_FALSE(Fraction(1, 2) < Fraction(1, 3));
}
ZTH_TEST("works", "printing: 3/4, -3/4, and a whole number without the denominator") {
    ZTH_EXPECT_EQ(str(Fraction(3, 4)), std::string("3/4"));
    ZTH_EXPECT_EQ(str(Fraction(-3, 4)), std::string("-3/4"));
    ZTH_EXPECT_EQ(str(Fraction(6, 3)), std::string("2"));
    ZTH_EXPECT_EQ(str(Fraction(0, 3)), std::string("0"));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "a zero denominator throws std::invalid_argument") {
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)Fraction(1, 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)Fraction(0, 0));
}
ZTH_TEST("robust", "dividing by a zero fraction throws std::domain_error (and by a non-zero one does not)") {
    ZTH_EXPECT_THROWS(std::domain_error, (void)(Fraction(1, 2) / Fraction(0)));
    ZTH_EXPECT_THROWS(std::domain_error, (void)(Fraction(1, 2) / Fraction(0, 5)));
    ZTH_EXPECT_NO_THROW((void)(Fraction(0) / Fraction(1, 2)));
}
ZTH_TEST("robust", "an integer works on either side of + - * /  (so the operators are not members)") {
    mixed_test<Fraction>();
}
ZTH_TEST("robust", "a whole number converts implicitly to Fraction, but Fraction -> double must be asked for") {
    ZTH_EXPECT_TRUE((std::is_convertible_v<int, Fraction>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<long long, Fraction>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<Fraction, double>));
    ZTH_EXPECT_TRUE((std::is_constructible_v<double, Fraction>));  // ... but static_cast<double>(f) works
}
ZTH_TEST("robust", "static_cast<double> gives the value (not the integer-division result)") {
    ZTH_EXPECT_NEAR(static_cast<double>(Fraction(1, 3)), 0.3333333333, 1e-9);
    ZTH_EXPECT_NEAR(static_cast<double>(Fraction(-7, 2)), -3.5, 1e-12);
    ZTH_EXPECT_NEAR(static_cast<double>(Fraction(0)), 0.0, 1e-12);
}
ZTH_TEST("robust", "multiplying cancels before multiplying: the straight-across products overflow, the answer does not") {
    // 4000000000/4000000007 * 4000000007/3999999999 = 4000000000/3999999999, but 4000000000 * 4000000007 is about 1.6e19
    ZTH_EXPECT_EQ(Fraction(4'000'000'000LL, 4'000'000'007LL) * Fraction(4'000'000'007LL, 3'999'999'999LL),
                  Fraction(4'000'000'000LL, 3'999'999'999LL));
}
ZTH_TEST("robust", "adding uses the least common denominator: 1/4000000000 + 3/4000000000 is 1/1000000000") {
    ZTH_EXPECT_EQ(Fraction(1, 4'000'000'000LL) + Fraction(3, 4'000'000'000LL), Fraction(1, 1'000'000'000LL));
    ZTH_EXPECT_EQ(Fraction(1, 4'000'000'000LL) - Fraction(1, 4'000'000'000LL), Fraction(0));
}
ZTH_TEST("robust", "comparison is exact: it must not go through double") {
    // 2999999999/3000000000 and 2999999998/2999999999 differ by about 1e-19, far below a double's precision
    const Fraction a(2'999'999'999LL, 3'000'000'000LL), b(2'999'999'998LL, 2'999'999'999LL);
    ZTH_EXPECT_TRUE(b < a);  // 1 - 1/2999999999 is smaller than 1 - 1/3000000000
    ZTH_EXPECT_TRUE(a > b);
    ZTH_EXPECT_TRUE(a != b);
    ZTH_EXPECT_FALSE(a == b);
}
ZTH_TEST("robust", "results are always normalized: negative results, negative * negative") {
    const Fraction d = Fraction(1, 3) - Fraction(1, 2);
    ZTH_EXPECT_EQ(d.numerator(), -1LL);
    ZTH_EXPECT_EQ(d.denominator(), 6LL);
    const Fraction p = Fraction(-1, 2) * Fraction(-2, 3);
    ZTH_EXPECT_EQ(p.numerator(), 1LL);
    ZTH_EXPECT_EQ(p.denominator(), 3LL);
    const Fraction q = Fraction(1, 2) / Fraction(-1, 4);
    ZTH_EXPECT_EQ(q.numerator(), -2LL);
    ZTH_EXPECT_EQ(q.denominator(), 1LL);
}
ZTH_TEST("robust", "compound assignment works and returns the object") {
    Fraction f(1, 2);
    f += Fraction(1, 3);
    ZTH_EXPECT_EQ(f, Fraction(5, 6));
    f -= Fraction(1, 6);
    ZTH_EXPECT_EQ(f, Fraction(2, 3));
    f *= Fraction(3, 2);
    ZTH_EXPECT_EQ(f, Fraction(1));
    f /= Fraction(1, 4);
    ZTH_EXPECT_EQ(f, Fraction(4));
    Fraction& r = (f += Fraction(1));
    ZTH_EXPECT_TRUE(&r == &f);
}
ZTH_TEST("robust", "all six comparison operators exist, and std::sort orders fractions") {
    if (!has_all_comparisons<Fraction>) {
        ::zth::fail("one of == != < <= > >= is missing");
        return;
    }
    std::vector<Fraction> v{Fraction(1, 2), Fraction(-1, 3), Fraction(3, 4), Fraction(1, 6), Fraction(-5, 2), Fraction(2, 4)};
    std::sort(v.begin(), v.end());
    const std::vector<Fraction> expected{Fraction(-5, 2), Fraction(-1, 3), Fraction(1, 6), Fraction(1, 2), Fraction(1, 2), Fraction(3, 4)};
    ZTH_EXPECT_EQ(v, expected);
}
ZTH_TEST("robust", "negative numbers print with the sign in front; the sum of ten tenths is exactly 1") {
    ZTH_EXPECT_EQ(str(Fraction(5, -10)), std::string("-1/2"));
    Fraction total;
    for (int i = 0; i < 10; ++i) total += Fraction(1, 10);
    ZTH_EXPECT_EQ(total, Fraction(1));
    ZTH_EXPECT_EQ(str(total), std::string("1"));
}
