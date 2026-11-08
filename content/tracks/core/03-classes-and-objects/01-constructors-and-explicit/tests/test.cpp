#include <climits>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "money.hpp"
#include "zth_test.hpp"

namespace {

// true if `f()` throws an Ex (or something derived from it); any other outcome, including a different exception, is false
template <class Ex, class F>
bool throws(F&& f) {
    try {
        f();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

const std::string kMax = "$92233720368547758.07";   // LLONG_MAX cents
const std::string kMin = "-$92233720368547758.08";  // LLONG_MIN cents

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a default Money is $0.00") {
    Money m;  // not const: a const default-initialized object needs every member initialized, which is checked separately below
    ZTH_EXPECT_EQ(m.cents(), 0LL);
    ZTH_EXPECT_EQ(m.to_string(), std::string("$0.00"));
}
ZTH_TEST("works", "Money(1234) is 1234 cents, $12.34") {
    const Money m(1234);
    ZTH_EXPECT_EQ(m.cents(), 1234LL);
    ZTH_EXPECT_EQ(m.to_string(), std::string("$12.34"));
}
ZTH_TEST("works", "Money(12, 34) is also 1234 cents (the two-argument constructor delegates)") {
    ZTH_EXPECT_EQ(Money(12, 34).cents(), 1234LL);
    ZTH_EXPECT_EQ(Money(3, 7).cents(), 307LL);
    ZTH_EXPECT_EQ(Money(0, 0).cents(), 0LL);
    ZTH_EXPECT_EQ(Money(0, 99).cents(), 99LL);
}
ZTH_TEST("works", "to_string pads the cents to two digits") {
    ZTH_EXPECT_EQ(Money(5).to_string(), std::string("$0.05"));
    ZTH_EXPECT_EQ(Money(100).to_string(), std::string("$1.00"));
    ZTH_EXPECT_EQ(Money(1999).to_string(), std::string("$19.99"));
    ZTH_EXPECT_EQ(Money(123456789).to_string(), std::string("$1234567.89"));
}
ZTH_TEST("works", "a debt prints with the minus sign in front of the $") {
    ZTH_EXPECT_EQ(Money(-5).to_string(), std::string("-$0.05"));
    ZTH_EXPECT_EQ(Money(-1234).to_string(), std::string("-$12.34"));
    ZTH_EXPECT_EQ(Money(-100).to_string(), std::string("-$1.00"));
}
ZTH_TEST("works", "plus adds amounts, including negative ones") {
    ZTH_EXPECT_EQ(Money(150).plus(Money(275)).cents(), 425LL);
    ZTH_EXPECT_EQ(Money(150).plus(Money(-200)).cents(), -50LL);
    ZTH_EXPECT_EQ(Money().plus(Money()).cents(), 0LL);
}
ZTH_TEST("works", "the cents constructor is explicit: a plain number must not turn into Money by itself") {
    ZTH_EXPECT_FALSE((std::is_convertible_v<long long, Money>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<int, Money>));
    ZTH_EXPECT_TRUE((std::is_constructible_v<Money, long long>));  // ... but asking for it is fine
    ZTH_EXPECT_TRUE((std::is_constructible_v<Money, long long, int>));
    ZTH_EXPECT_TRUE(std::is_default_constructible_v<Money>);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "cents outside 0..99 throw std::invalid_argument, and so does negative dollars") {
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(1, 100); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(1, 1000); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(1, -1); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(-1, 0); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(-5, 50); }));
}
ZTH_TEST("robust", "the edges of the valid range are accepted") {
    ZTH_EXPECT_FALSE(throws<std::exception>([] { (void)Money(0, 0); }));
    ZTH_EXPECT_FALSE(throws<std::exception>([] { (void)Money(0, 99); }));
    ZTH_EXPECT_FALSE(throws<std::exception>([] { (void)Money(7, 0); }));
    ZTH_EXPECT_EQ(Money(1, 99).cents(), 199LL);
}
ZTH_TEST("robust", "an amount that does not fit in a long long throws std::overflow_error; the largest one that fits does not") {
    ZTH_EXPECT_EQ(Money(92233720368547758LL, 7).cents(), LLONG_MAX);
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(92233720368547758LL, 8); }));
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(92233720368547759LL, 0); }));
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(LLONG_MAX, 0); }));
}
ZTH_TEST("robust", "an invalid argument is reported before an overflow: Money(LLONG_MAX, 100) is invalid_argument") {
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(LLONG_MAX, 100); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)Money(LLONG_MAX, -1); }));
}
ZTH_TEST("robust", "to_string works at both ends of the range (negating LLONG_MIN would overflow)") {
    ZTH_EXPECT_EQ(Money(LLONG_MAX).to_string(), kMax);
    ZTH_EXPECT_EQ(Money(LLONG_MIN).to_string(), kMin);
    ZTH_EXPECT_EQ(Money(LLONG_MIN + 1).to_string(), std::string("-$92233720368547758.07"));
}
ZTH_TEST("robust", "plus throws std::overflow_error instead of wrapping around") {
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(LLONG_MAX).plus(Money(1)); }));
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(LLONG_MIN).plus(Money(-1)); }));
    ZTH_EXPECT_TRUE(throws<std::overflow_error>([] { (void)Money(1).plus(Money(LLONG_MAX)); }));
}
ZTH_TEST("robust", "plus is exact right up to the limits") {
    ZTH_EXPECT_EQ(Money(LLONG_MAX).plus(Money(0)).cents(), LLONG_MAX);
    ZTH_EXPECT_EQ(Money(LLONG_MAX).plus(Money(-1)).cents(), LLONG_MAX - 1);
    ZTH_EXPECT_EQ(Money(LLONG_MIN).plus(Money(1)).cents(), LLONG_MIN + 1);
    ZTH_EXPECT_EQ(Money(LLONG_MIN).plus(Money(LLONG_MAX)).cents(), -1LL);
    ZTH_EXPECT_EQ(Money(LLONG_MAX - 5).plus(Money(5)).cents(), LLONG_MAX);
}
ZTH_TEST("robust", "a default-constructed Money is zero even when the memory held garbage before") {
    alignas(Money) unsigned char buf[sizeof(Money)];
    std::memset(buf, 0xAB, sizeof buf);
    Money* m = new (buf) Money;  // default-initialization: only the class itself can give cents_ a value
    const long long c = m->cents();
    m->~Money();
    ZTH_EXPECT_EQ(c, 0LL);
}
ZTH_TEST("robust", "brace initialization picks the same constructors") {
    const Money a{250};
    const Money b{5, 50};
    ZTH_EXPECT_EQ(a.cents(), 250LL);
    ZTH_EXPECT_EQ(b.cents(), 550LL);
    const std::vector<Money> v(3);  // needs a default constructor
    ZTH_EXPECT_EQ(v.size(), std::size_t{3});
    ZTH_EXPECT_EQ(v[2].cents(), 0LL);
}
