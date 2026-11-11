#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "money.hpp"
#include "zth_test.hpp"

using namespace fin;

namespace {

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

Money M(std::int64_t cents) { return Money::from_cents(cents); }

std::string shares(const std::vector<Money>& v) {
    std::string s;
    for (const Money& m : v) s += (s.empty() ? "" : ",") + std::to_string(m.cents());
    return s;
}

// expect `cents / n` rounded in all three modes
void expect_div(std::int64_t cents, std::int64_t n, std::int64_t down, std::int64_t half_up, std::int64_t half_even) {
    const struct { Rounding mode; std::int64_t want; const char* name; } cases[] = {{Rounding::Down, down, "Down"}, {Rounding::HalfUp, half_up, "HalfUp"}, {Rounding::HalfEven, half_even, "HalfEven"}};
    for (const auto& c : cases) {
        const auto r = M(cents).divided(n, c.mode);
        if (!r || r->cents() != c.want) ::zth::fail(std::to_string(cents) + " / " + std::to_string(n) + " (" + c.name + ") should be " + std::to_string(c.want) + ", got " + (r ? std::to_string(r->cents()) : std::string("nullopt")));
    }
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "from_cents, cents, comparison; the default amount is zero") {
    ZTH_EXPECT_EQ(Money().cents(), 0LL);
    ZTH_EXPECT_EQ(M(1234).cents(), 1234LL);
    ZTH_EXPECT_TRUE(M(5) == M(5));
    ZTH_EXPECT_TRUE(M(5) != M(6));
    ZTH_EXPECT_TRUE(M(-5) < M(3));
    ZTH_EXPECT_TRUE(M(7) > M(-7));
    ZTH_EXPECT_TRUE(M(7) >= M(7) && M(7) <= M(7));
}
ZTH_TEST("works", "checked add, subtract and multiply give exact results") {
    ZTH_EXPECT_EQ(checked_add(M(10), M(20))->cents(), 30LL);
    ZTH_EXPECT_EQ(checked_add(M(-10), M(3))->cents(), -7LL);
    ZTH_EXPECT_EQ(checked_sub(M(10), M(20))->cents(), -10LL);
    ZTH_EXPECT_EQ(checked_mul(M(125), 4)->cents(), 500LL);
    ZTH_EXPECT_EQ(checked_mul(M(125), -3)->cents(), -375LL);
    ZTH_EXPECT_EQ(checked_mul(M(125), 0)->cents(), 0LL);
}
ZTH_TEST("works", "0.10 + 0.20 is exactly 0.30 (no floating point)") {
    const auto sum = checked_add(*Money::parse("0.10"), *Money::parse("0.20"));
    ZTH_EXPECT_TRUE(sum.has_value());
    ZTH_EXPECT_TRUE(*sum == *Money::parse("0.30"));
    ZTH_EXPECT_EQ(sum->to_string(), std::string("$0.30"));
}
ZTH_TEST("works", "to_string: dollars, two digits of cents, a sign in front of the dollar sign") {
    ZTH_EXPECT_EQ(M(0).to_string(), std::string("$0.00"));
    ZTH_EXPECT_EQ(M(5).to_string(), std::string("$0.05"));
    ZTH_EXPECT_EQ(M(50).to_string(), std::string("$0.50"));
    ZTH_EXPECT_EQ(M(1234).to_string(), std::string("$12.34"));
    ZTH_EXPECT_EQ(M(100).to_string(), std::string("$1.00"));
    ZTH_EXPECT_EQ(M(123456789).to_string(), std::string("$1234567.89"));
    ZTH_EXPECT_EQ(M(-5).to_string(), std::string("-$0.05"));
    ZTH_EXPECT_EQ(M(-1234).to_string(), std::string("-$12.34"));
}
ZTH_TEST("works", "parse: with and without $, with 0, 1 or 2 decimals, negative") {
    ZTH_EXPECT_EQ(Money::parse("12.34")->cents(), 1234LL);
    ZTH_EXPECT_EQ(Money::parse("$12.34")->cents(), 1234LL);
    ZTH_EXPECT_EQ(Money::parse("-$12.34")->cents(), -1234LL);
    ZTH_EXPECT_EQ(Money::parse("-12.34")->cents(), -1234LL);
    ZTH_EXPECT_EQ(Money::parse("12")->cents(), 1200LL);
    ZTH_EXPECT_EQ(Money::parse("$7")->cents(), 700LL);
    ZTH_EXPECT_EQ(Money::parse("12.3")->cents(), 1230LL);
    ZTH_EXPECT_EQ(Money::parse("0.05")->cents(), 5LL);
    ZTH_EXPECT_EQ(Money::parse("0.5")->cents(), 50LL);
    ZTH_EXPECT_EQ(Money::parse("007.01")->cents(), 701LL);
    ZTH_EXPECT_EQ(Money::parse("-0")->cents(), 0LL);
}
ZTH_TEST("works", "divided rounds to a whole cent: Down, HalfUp, HalfEven") {
    expect_div(10, 2, 5, 5, 5);      // exact
    expect_div(10, 3, 3, 3, 3);      // 3.33
    expect_div(11, 3, 3, 4, 4);      // 3.67
    expect_div(5, 2, 2, 3, 2);       // 2.5: half-up goes up, half-even goes to the even 2
    expect_div(7, 2, 3, 4, 4);       // 3.5: both go to 4
    expect_div(9, 2, 4, 5, 4);       // 4.5
    expect_div(1, 4, 0, 0, 0);       // 0.25
    expect_div(3, 4, 0, 1, 1);       // 0.75
}
ZTH_TEST("works", "negative amounts round symmetrically: Down is toward zero, HalfUp is away from zero") {
    expect_div(-5, 2, -2, -3, -2);
    expect_div(-7, 2, -3, -4, -4);
    expect_div(-11, 3, -3, -4, -4);
    expect_div(-10, 3, -3, -3, -3);
    expect_div(5, -2, -2, -3, -2);
    expect_div(-5, -2, 2, 3, 2);
    expect_div(-9, 2, -4, -5, -4);
}
ZTH_TEST("works", "percent in basis points, rounded") {
    ZTH_EXPECT_EQ(M(10000).percent(725, Rounding::HalfUp)->cents(), 725LL);      // $100.00 * 7.25 %
    ZTH_EXPECT_EQ(M(1999).percent(825, Rounding::HalfUp)->cents(), 165LL);       // 164.1675 -> 164? no: 1999*825/10000 = 164.9175 -> 165
    ZTH_EXPECT_EQ(M(1999).percent(825, Rounding::Down)->cents(), 164LL);
    ZTH_EXPECT_EQ(M(1000).percent(50, Rounding::HalfEven)->cents(), 5LL);        // 0.5 %
    ZTH_EXPECT_EQ(M(10).percent(5000, Rounding::HalfEven)->cents(), 5LL);        // 50 %
    ZTH_EXPECT_EQ(M(5).percent(5000, Rounding::HalfEven)->cents(), 2LL);         // 2.5 -> 2
    ZTH_EXPECT_EQ(M(5).percent(5000, Rounding::HalfUp)->cents(), 3LL);
    ZTH_EXPECT_EQ(M(-5).percent(5000, Rounding::HalfUp)->cents(), -3LL);
    ZTH_EXPECT_EQ(M(2000).percent(-1000, Rounding::Down)->cents(), -200LL);      // a negative rate
}
ZTH_TEST("works", "allocate splits so that the shares add up exactly, larger shares first") {
    ZTH_EXPECT_EQ(shares(M(100).allocate(3)), std::string("34,33,33"));
    ZTH_EXPECT_EQ(shares(M(10000).allocate(3)), std::string("3334,3333,3333"));
    ZTH_EXPECT_EQ(shares(M(101).allocate(4)), std::string("26,25,25,25"));
    ZTH_EXPECT_EQ(shares(M(1).allocate(3)), std::string("1,0,0"));
    ZTH_EXPECT_EQ(shares(M(0).allocate(3)), std::string("0,0,0"));
    ZTH_EXPECT_EQ(shares(M(90).allocate(3)), std::string("30,30,30"));
    ZTH_EXPECT_EQ(shares(M(7).allocate(1)), std::string("7"));
    ZTH_EXPECT_EQ(shares(M(-100).allocate(3)), std::string("-34,-33,-33"));
}
ZTH_TEST("works", "allocate_by_ratio: proportional shares, no cent lost") {
    ZTH_EXPECT_EQ(shares(M(1000).allocate_by_ratio({1, 2, 2})), std::string("200,400,400"));
    ZTH_EXPECT_EQ(shares(M(100).allocate_by_ratio({1, 1, 1})), std::string("34,33,33"));
    ZTH_EXPECT_EQ(shares(M(100).allocate_by_ratio({70, 20, 10})), std::string("70,20,10"));
    ZTH_EXPECT_EQ(shares(M(10).allocate_by_ratio({1, 1, 1})), std::string("4,3,3"));
    ZTH_EXPECT_EQ(shares(M(100).allocate_by_ratio({0, 3, 0, 1})), std::string("0,75,0,25"));
    ZTH_EXPECT_EQ(shares(M(5).allocate_by_ratio({3, 1})), std::string("4,1"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "overflow in add, subtract and multiply returns nullopt, exactly at the edges") {
    ZTH_EXPECT_TRUE(checked_add(M(kMax), M(1)) == std::nullopt);
    ZTH_EXPECT_TRUE(checked_add(M(kMax), M(0)).has_value());
    ZTH_EXPECT_EQ(checked_add(M(kMax - 5), M(5))->cents(), kMax);
    ZTH_EXPECT_TRUE(checked_add(M(kMin), M(-1)) == std::nullopt);
    ZTH_EXPECT_EQ(checked_add(M(kMin), M(kMax))->cents(), -1LL);
    ZTH_EXPECT_TRUE(checked_sub(M(kMin), M(1)) == std::nullopt);
    ZTH_EXPECT_TRUE(checked_sub(M(0), M(kMin)) == std::nullopt);
    ZTH_EXPECT_EQ(checked_sub(M(-1), M(kMax))->cents(), kMin);
    ZTH_EXPECT_TRUE(checked_sub(M(kMax), M(-1)) == std::nullopt);
    ZTH_EXPECT_TRUE(checked_mul(M(kMax), 2) == std::nullopt);
    ZTH_EXPECT_TRUE(checked_mul(M(kMin), -1) == std::nullopt);
    ZTH_EXPECT_TRUE(checked_mul(M(kMin), 1).has_value());
    ZTH_EXPECT_TRUE(checked_mul(M(3037000500LL), 3037000500LL) == std::nullopt);   // just over 2^63
    ZTH_EXPECT_EQ(checked_mul(M(3037000499LL), 3037000499LL)->cents(), 9223372030926249001LL);
    ZTH_EXPECT_EQ(checked_mul(M(kMax), 1)->cents(), kMax);
    ZTH_EXPECT_EQ(checked_mul(M(kMax), -1)->cents(), -kMax);
    ZTH_EXPECT_EQ(checked_mul(M(0), kMax)->cents(), 0LL);
    ZTH_EXPECT_TRUE(checked_mul(M(2), kMin) == std::nullopt);
}
ZTH_TEST("robust", "negated and abs: INT64_MIN has no positive counterpart") {
    ZTH_EXPECT_EQ(M(5).negated()->cents(), -5LL);
    ZTH_EXPECT_EQ(M(-5).negated()->cents(), 5LL);
    ZTH_EXPECT_EQ(M(0).negated()->cents(), 0LL);
    ZTH_EXPECT_TRUE(M(kMin).negated() == std::nullopt);
    ZTH_EXPECT_EQ(M(kMax).negated()->cents(), -kMax);
    ZTH_EXPECT_EQ(M(-5).abs()->cents(), 5LL);
    ZTH_EXPECT_EQ(M(5).abs()->cents(), 5LL);
    ZTH_EXPECT_TRUE(M(kMin).abs() == std::nullopt);
    ZTH_EXPECT_EQ(M(kMin + 1).abs()->cents(), kMax);
}
ZTH_TEST("robust", "divided: zero divisor and INT64_MIN / -1 are nullopt; large values and divisors are exact") {
    ZTH_EXPECT_TRUE(M(10).divided(0, Rounding::Down) == std::nullopt);
    ZTH_EXPECT_TRUE(M(kMin).divided(-1, Rounding::Down) == std::nullopt);
    ZTH_EXPECT_EQ(M(kMin).divided(1, Rounding::Down)->cents(), kMin);
    ZTH_EXPECT_EQ(M(kMax).divided(1, Rounding::HalfEven)->cents(), kMax);
    ZTH_EXPECT_EQ(M(kMax).divided(kMax, Rounding::HalfUp)->cents(), 1LL);
    ZTH_EXPECT_EQ(M(kMax).divided(2, Rounding::HalfUp)->cents(), 4611686018427387904LL);   // 4611686018427387903.5
    ZTH_EXPECT_EQ(M(kMax).divided(2, Rounding::HalfEven)->cents(), 4611686018427387904LL); // the even neighbour
    ZTH_EXPECT_EQ(M(kMax).divided(2, Rounding::Down)->cents(), 4611686018427387903LL);
    ZTH_EXPECT_EQ(M(kMin).divided(2, Rounding::HalfEven)->cents(), kMin / 2);
    ZTH_EXPECT_EQ(M(1).divided(kMax, Rounding::HalfUp)->cents(), 0LL);
    ZTH_EXPECT_EQ(M(kMax).divided(-1, Rounding::Down)->cents(), -kMax);
}
ZTH_TEST("robust", "half-even ties alternate: x.5 goes to the even neighbour for every x from -6 to 6") {
    for (int twice = -13; twice <= 13; twice += 2) {   // twice/2 = x.5
        const auto r = M(twice).divided(2, Rounding::HalfEven);
        const long long lo = (twice - 1) / 2, hi = lo + 1;   // the two neighbours (careful with negatives)
        const long long floor_v = twice >= 0 ? twice / 2 : -((-twice + 1) / 2);
        const long long nearest_even = (floor_v % 2 == 0) ? floor_v : floor_v + 1;
        (void)lo;
        (void)hi;
        ZTH_EXPECT_EQ(r->cents(), nearest_even);
    }
}
ZTH_TEST("robust", "percent needs a wider intermediate: amount * basis points can exceed int64 while the result fits") {
    ZTH_EXPECT_EQ(M(kMax).percent(10000, Rounding::Down)->cents(), kMax);          // 100 %
    ZTH_EXPECT_EQ(M(kMax).percent(5000, Rounding::Down)->cents(), 4611686018427387903LL);
    ZTH_EXPECT_EQ(M(kMax).percent(1, Rounding::Down)->cents(), 922337203685477LL);  // 0.01 %
    ZTH_EXPECT_EQ(M(kMin).percent(10000, Rounding::Down)->cents(), kMin);
    ZTH_EXPECT_TRUE(M(kMax).percent(10001, Rounding::Down) == std::nullopt);          // 100.01 %: does not fit
    ZTH_EXPECT_TRUE(M(kMax).percent(20000, Rounding::HalfUp) == std::nullopt);
    ZTH_EXPECT_EQ(M(5000000000LL).percent(4000000000LL, Rounding::Down)->cents(), 2000000000000000LL);
    ZTH_EXPECT_EQ(M(123).percent(0, Rounding::HalfEven)->cents(), 0LL);
}
ZTH_TEST("robust", "allocate invariants: the sum is exact, shares differ by at most a cent, larger first (positive and negative amounts)") {
    ::zth::Lcg rng(99);
    for (int i = 0; i < 3000; ++i) {
        const std::int64_t amount = (static_cast<std::int64_t>(rng.next()) << 20) % 1000000007LL * (rng.below(2) ? 1 : -1);
        const int parts = 1 + rng.below(40);
        const auto v = M(amount).allocate(parts);
        ZTH_EXPECT_EQ(v.size(), static_cast<std::size_t>(parts));
        std::int64_t sum = 0;
        for (std::size_t k = 0; k < v.size(); ++k) {
            sum += v[k].cents();
            if (k > 0) {
                const std::int64_t diff = amount >= 0 ? v[k - 1].cents() - v[k].cents() : v[k].cents() - v[k - 1].cents();
                ZTH_EXPECT_TRUE(diff == 0 || diff == 1);   // never a smaller share before a larger one
            }
        }
        ZTH_EXPECT_EQ(sum, amount);
        const std::int64_t lo = std::min(v.front().cents(), v.back().cents()), hi = std::max(v.front().cents(), v.back().cents());
        ZTH_EXPECT_TRUE(hi - lo <= 1);
    }
}
ZTH_TEST("robust", "allocate with bad arguments, with the extreme amounts, and allocate_by_ratio's rules") {
    ZTH_EXPECT_TRUE(M(100).allocate(0).empty());
    ZTH_EXPECT_TRUE(M(100).allocate(-3).empty());
    std::int64_t sum = 0;
    for (const Money& m : M(kMax).allocate(7)) sum += m.cents();   // may wrap in the test only if the shares are wrong
    ZTH_EXPECT_EQ(shares(M(kMax).allocate(2)), std::string("4611686018427387904,4611686018427387903"));
    ZTH_EXPECT_EQ(shares(M(kMin).allocate(2)), std::string("-4611686018427387904,-4611686018427387904"));
    (void)sum;
    ZTH_EXPECT_TRUE(M(100).allocate_by_ratio({}).empty());
    ZTH_EXPECT_TRUE(M(100).allocate_by_ratio({0, 0}).empty());
    ZTH_EXPECT_TRUE(M(100).allocate_by_ratio({1, -1}).empty());
    ZTH_EXPECT_TRUE(M(100).allocate_by_ratio({3, -1}).empty());
    ZTH_EXPECT_EQ(shares(M(0).allocate_by_ratio({1, 2})), std::string("0,0"));
    ZTH_EXPECT_EQ(shares(M(-100).allocate_by_ratio({1, 1, 1})), std::string("-34,-33,-33"));
    ZTH_EXPECT_EQ(shares(M(-10).allocate_by_ratio({3, 1})), std::string("-8,-2"));
    ZTH_EXPECT_EQ(shares(M(kMax).allocate_by_ratio({1, 1})), std::string("4611686018427387904,4611686018427387903"));
    ZTH_EXPECT_EQ(shares(M(1).allocate_by_ratio({5, 5, 5})), std::string("1,0,0"));
    ZTH_EXPECT_EQ(shares(M(1000000000000LL).allocate_by_ratio({1000000000LL, 3000000000LL})), std::string("250000000000,750000000000"));
}
ZTH_TEST("robust", "to_string and parse at the extremes: INT64_MIN and INT64_MAX round trip") {
    ZTH_EXPECT_EQ(M(kMax).to_string(), std::string("$92233720368547758.07"));
    ZTH_EXPECT_EQ(M(kMin).to_string(), std::string("-$92233720368547758.08"));
    ZTH_EXPECT_EQ(Money::parse("$92233720368547758.07")->cents(), kMax);
    ZTH_EXPECT_EQ(Money::parse("-$92233720368547758.08")->cents(), kMin);
    ZTH_EXPECT_TRUE(Money::parse("$92233720368547758.08") == std::nullopt);
    ZTH_EXPECT_TRUE(Money::parse("-$92233720368547758.09") == std::nullopt);
    ZTH_EXPECT_TRUE(Money::parse("$92233720368547759") == std::nullopt);
    ZTH_EXPECT_TRUE(Money::parse("99999999999999999999999999") == std::nullopt);
    ZTH_EXPECT_TRUE(Money::parse("0000000000000000000000000000000000012.34").has_value());
}
ZTH_TEST("robust", "parse is strict: everything outside the grammar is nullopt") {
    for (const char* bad : {"", "-", "$", "-$", "$-5", "+5", " 5", "5 ", "5.", ".5", "5.123", "5.1.2", "1,234.50", "$$5", "--5", "5$", "five", "5.5x", "0x10", "1e3", "\t5", "5\n", "$ 5", "5,00", "5.-1", "--", "-.5", "$.50"}) {
        if (Money::parse(bad).has_value()) {
            ::zth::fail(std::string("\"") + bad + "\" should not parse");
            return;
        }
    }
}
ZTH_TEST_TIMEOUT("robust", "2000 random amounts survive to_string -> parse, and the text has the right shape", 10) {
    ::zth::Lcg rng(4);
    for (int i = 0; i < 2000; ++i) {
        const std::int64_t cents = (static_cast<std::int64_t>(rng.next()) * rng.next()) * (rng.below(2) ? 1 : -1) % (kMax / 2) + rng.below(200) - 100;
        const std::string text = M(cents).to_string();
        const auto back = Money::parse(text);
        ZTH_EXPECT_TRUE(back.has_value());
        ZTH_EXPECT_EQ(back->cents(), cents);
        const std::size_t dot = text.find('.');
        ZTH_EXPECT_TRUE(dot != std::string::npos && text.size() - dot == 3);
        ZTH_EXPECT_TRUE(text.find('$') == (cents < 0 ? std::size_t{1} : std::size_t{0}));
    }
}
ZTH_TEST("robust", "rounding matches an exact reference for 4000 random divisions in all three modes") {
    ::zth::Lcg rng(66);
    for (int i = 0; i < 4000; ++i) {
        const std::int64_t cents = (rng.below(2001) - 1000) * (1 + rng.below(50));
        std::int64_t n = rng.below(41) - 20;
        if (n == 0) n = 7;
        // reference in doubles-free integer maths: floor division by hand
        const long long q = cents / n, r = cents % n;
        long long down = q, up = q, even = q;
        if (r != 0) {
            const long long twice = (r < 0 ? -r : r) * 2, mag = n < 0 ? -n : n;
            const long long away = ((cents < 0) != (n < 0)) ? -1 : 1;
            if (twice > mag) up = even = q + away;
            else if (twice == mag) {
                up = q + away;
                even = (q % 2 == 0) ? q : q + away;
            }
        }
        ZTH_EXPECT_EQ(M(cents).divided(n, Rounding::Down)->cents(), down);
        ZTH_EXPECT_EQ(M(cents).divided(n, Rounding::HalfUp)->cents(), up);
        ZTH_EXPECT_EQ(M(cents).divided(n, Rounding::HalfEven)->cents(), even);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "the type is a plain 8-byte value: trivially copyable, no allocation in arithmetic or parsing", 10) {
    ZTH_EXPECT_EQ(sizeof(Money), std::size_t{8});
    ZTH_EXPECT_TRUE(std::is_trivially_copyable_v<Money>);
    zth::mem::Scope scope;
    long long acc = 0;
    for (int i = 0; i < 300000; ++i) {
        const Money a = M(i), b = M(i * 7 + 1);
        acc += checked_add(a, b)->cents() + checked_mul(a, 3)->cents() + a.divided(3, Rounding::HalfEven)->cents() + a.percent(825, Rounding::HalfUp)->cents();
        acc += Money::parse("12.34")->cents() + Money::parse("-$5.5")->cents() + Money::parse("-$12345678901234.56")->cents();
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(acc != 0);
}
ZTH_TEST_TIMEOUT("fast", "to_string allocates at most once per call (200000 calls)", 10) {
    zth::mem::Scope scope;
    std::size_t total = 0;
    for (int i = 0; i < 200000; ++i) total += M(static_cast<std::int64_t>(i) * 1000003).to_string().size();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 200000);
    ZTH_EXPECT_TRUE(total > 0);
}
ZTH_TEST_TIMEOUT("fast", "allocate makes one allocation per call (the result vector): 100000 splits of $1000 in 7", 10) {
    zth::mem::Scope scope;
    std::int64_t sum = 0;
    for (int i = 0; i < 100000; ++i) {
        for (const Money& m : M(100000 + i).allocate(7)) sum += m.cents();
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 100000);
    ZTH_EXPECT_TRUE(sum > 0);
}
ZTH_TEST_TIMEOUT("fast", "a million additions of 0.10 give exactly 100000.00", 10) {
    Money total;
    const Money dime = *Money::parse("0.10");
    for (int i = 0; i < 1000000; ++i) total = *checked_add(total, dime);
    ZTH_EXPECT_EQ(total.to_string(), std::string("$100000.00"));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no floating point anywhere in money.hpp: not even for the intermediate of percent") {
    ZTH_EXPECT_NO_RAW_MEMORY("money.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("money.hpp"));
    for (const char* word : {"float", "double", "stod", "stof", "stold", "strtod", "atof", "pow", "round", "floor", "ceil", "lround", "llround", "nearbyint", "rint"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("money.hpp uses `") + word + "`: money is integer cents");
            return;
        }
    }
}
ZTH_TEST("clean", "no exceptions and no throwing conversions: stoi / stoll / stol are out (use std::from_chars), no goto") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("money.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "stoi", "stol", "stoll", "stoul", "stoull", "atoi", "atol", "atoll", "sscanf", "stringstream", "istringstream"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("money.hpp uses `") + word + "`");
            return;
        }
    }
}
ZTH_TEST("clean", "failures are std::optional, not magic values or exceptions: checked_add / divided / percent / parse return std::optional") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("money.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "optional").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "nullopt").size() >= 1);
}
