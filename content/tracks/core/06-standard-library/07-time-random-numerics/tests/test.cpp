#define ZTH_TRACK_ALLOCATIONS
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "sim_tools.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

namespace {

using S = std::string;
using V = std::vector<int>;

// a clock the test controls
struct FakeClock {
    using rep = long long;
    using period = std::nano;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<FakeClock>;
    static constexpr bool is_steady [[maybe_unused]] = true;
    static inline long long ticks = 0;
    static time_point now() { return time_point(duration(ticks)); }
    static void advance(std::chrono::milliseconds d) { ticks += std::chrono::duration_cast<duration>(d).count(); }
};

// an engine that returns a scripted sequence of 32-bit values and counts the calls
struct ScriptedEngine {
    using result_type = std::uint32_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return 0xFFFFFFFFu; }
    std::vector<std::uint32_t> script;
    std::size_t next = 0;
    int calls = 0;
    explicit ScriptedEngine(std::vector<std::uint32_t> s) : script(std::move(s)) {}
    result_type operator()() {
        ++calls;
        return script[next++ % script.size()];
    }
};

// the reference answer for a die roll, written separately from the solution
int reference_roll(std::mt19937& rng, std::uint32_t sides) {
    const std::uint64_t limit = (std::uint64_t{1} << 32) - (std::uint64_t{1} << 32) % sides;
    for (;;) {
        const std::uint64_t r = rng();
        if (r < limit) return static_cast<int>(r % sides) + 1;
    }
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "total_time adds minutes, seconds and milliseconds") {
    ZTH_EXPECT_EQ(total_time(1min, 30s, 250ms).count(), 90250LL);
    ZTH_EXPECT_EQ(total_time(0min, 0s, 0ms).count(), 0LL);
    ZTH_EXPECT_EQ(total_time(2min, 0s, 5ms).count(), 120005LL);
}
ZTH_TEST("works", "to_fractional_seconds keeps the fraction") {
    ZTH_EXPECT_NEAR(to_fractional_seconds(1500ms), 1.5, 1e-12);
    ZTH_EXPECT_NEAR(to_fractional_seconds(250ms), 0.25, 1e-12);
    ZTH_EXPECT_NEAR(to_fractional_seconds(2000ms), 2.0, 1e-12);
}
ZTH_TEST("works", "pretty_duration: hours, minutes and seconds") {
    ZTH_EXPECT_EQ(pretty_duration(3723004ms), S("1h 02m 03s"));
    ZTH_EXPECT_EQ(pretty_duration(0ms), S("0h 00m 00s"));
    ZTH_EXPECT_EQ(pretty_duration(59s), S("0h 00m 59s"));
    ZTH_EXPECT_EQ(pretty_duration(61min), S("1h 01m 00s"));
}
ZTH_TEST("works", "Stopwatch measures with the clock it is given") {
    FakeClock::ticks = 1000000;
    Stopwatch<FakeClock> w;
    w.start();
    FakeClock::advance(250ms);
    ZTH_EXPECT_EQ(w.elapsed().count(), 250LL);
    FakeClock::advance(1s);
    ZTH_EXPECT_EQ(w.elapsed().count(), 1250LL);
}
ZTH_TEST("works", "Stopwatch lap gives the time since the previous lap") {
    FakeClock::ticks = 0;
    Stopwatch<FakeClock> w;
    w.start();
    FakeClock::advance(100ms);
    ZTH_EXPECT_EQ(w.lap().count(), 100LL);
    FakeClock::advance(40ms);
    ZTH_EXPECT_EQ(w.lap().count(), 40LL);
    FakeClock::advance(7ms);
    ZTH_EXPECT_EQ(w.elapsed().count(), 7LL);  // elapsed counts from the last lap too: it restarted the clock
}
ZTH_TEST("works", "uniform_below with an unbiased engine value is the remainder") {
    ScriptedEngine e({7});
    ZTH_EXPECT_EQ(uniform_below(e, 5), 2u);
    ZTH_EXPECT_EQ(e.calls, 1);
}
ZTH_TEST("works", "uniform_below REJECTS values from the incomplete top block instead of using them") {
    ScriptedEngine e({0xFFFFFFFFu, 7});  // 2^32 % 3 == 1: the top value 0xFFFFFFFF must be thrown away
    ZTH_EXPECT_EQ(uniform_below(e, 3), 1u);
    ZTH_EXPECT_EQ(e.calls, 2);
}
ZTH_TEST("works", "simulate_rolls: the same arguments give the same rolls, always in 1..sides") {
    const V a = simulate_rolls(42, 200, 6);
    const V b = simulate_rolls(42, 200, 6);
    ZTH_EXPECT_TRUE(a == b);
    ZTH_EXPECT_EQ(a.size(), std::size_t{200});
    for (int r : a) {
        if (r < 1 || r > 6) {
            ::zth::fail("a roll of " + std::to_string(r) + " is not in 1..6");
            return;
        }
    }
}
ZTH_TEST("works", "simulate_rolls matches the reference algorithm on a std::mt19937") {
    std::mt19937 rng(2024);
    V expected;
    for (int i = 0; i < 50; ++i) expected.push_back(reference_roll(rng, 20));
    ZTH_EXPECT_TRUE((simulate_rolls(2024, 50, 20) == expected));
}
ZTH_TEST("works", "shuffle_deterministic keeps the same elements and is reproducible") {
    V a{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    V b = a;
    std::mt19937 r1(5), r2(5);
    shuffle_deterministic(a, r1);
    shuffle_deterministic(b, r2);
    ZTH_EXPECT_TRUE(a == b);
    V sorted = a;
    std::sort(sorted.begin(), sorted.end());
    ZTH_EXPECT_TRUE((sorted == V{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    ZTH_EXPECT_FALSE((a == V{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}
ZTH_TEST("works", "prefix_sums are the running totals") {
    const auto s = prefix_sums({1, 2, 3, 4});
    ZTH_EXPECT_TRUE((s == std::vector<long long>{1, 3, 6, 10}));
}
ZTH_TEST("works", "dot_product and gcd_of_all") {
    ZTH_EXPECT_EQ(dot_product({1, 2, 3}, {4, 5, 6}), 32LL);
    ZTH_EXPECT_EQ(gcd_of_all({12, 18, 30}), 6);
    ZTH_EXPECT_EQ(gcd_of_all({7}), 7);
}
ZTH_TEST("works", "nearly_equal: rounding errors are tolerated, real differences are not") {
    ZTH_EXPECT_FALSE(0.1 + 0.2 == 0.3);  // the reason this function exists
    ZTH_EXPECT_TRUE(nearly_equal(0.1 + 0.2, 0.3));
    ZTH_EXPECT_FALSE(nearly_equal(1.0, 1.001));
    ZTH_EXPECT_TRUE(nearly_equal(5.0, 5.0));
}
ZTH_TEST("works", "sum_accurate of a few numbers") {
    ZTH_EXPECT_NEAR(sum_accurate({1.5, 2.5, 3.0}), 7.0, 1e-12);
    ZTH_EXPECT_NEAR(sum_accurate({}), 0.0, 1e-12);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "durations of different units: the result is exact") {
    ZTH_EXPECT_EQ(total_time(1min, 1s, 1ms).count(), 61001LL);
    ZTH_EXPECT_EQ(total_time(-1min, 90s, 0ms).count(), 30000LL);
}
ZTH_TEST("robust", "to_fractional_seconds of tiny, zero and negative durations") {
    ZTH_EXPECT_NEAR(to_fractional_seconds(1ms), 0.001, 1e-15);
    ZTH_EXPECT_NEAR(to_fractional_seconds(0ms), 0.0, 1e-15);
    ZTH_EXPECT_NEAR(to_fractional_seconds(-1500ms), -1.5, 1e-12);
}
ZTH_TEST("robust", "pretty_duration: the milliseconds are dropped, more than 24 hours, and negative durations") {
    ZTH_EXPECT_EQ(pretty_duration(999ms), S("0h 00m 00s"));
    ZTH_EXPECT_EQ(pretty_duration(1999ms), S("0h 00m 01s"));
    ZTH_EXPECT_EQ(pretty_duration(100h + 5min + 9s), S("100h 05m 09s"));
    ZTH_EXPECT_EQ(pretty_duration(-1500ms), S("-0h 00m 01s"));
    ZTH_EXPECT_EQ(pretty_duration(-3723s), S("-1h 02m 03s"));
}
ZTH_TEST("robust", "pretty_duration at the unit boundaries") {
    ZTH_EXPECT_EQ(pretty_duration(60s), S("0h 01m 00s"));
    ZTH_EXPECT_EQ(pretty_duration(3599s), S("0h 59m 59s"));
    ZTH_EXPECT_EQ(pretty_duration(3600s), S("1h 00m 00s"));
}
ZTH_TEST("robust", "Stopwatch: a time that is not a whole number of milliseconds is truncated, and two stopwatches are independent") {
    FakeClock::ticks = 0;
    Stopwatch<FakeClock> a;
    a.start();
    FakeClock::ticks += 1999999;  // 1.999999 ms
    ZTH_EXPECT_EQ(a.elapsed().count(), 1LL);
    Stopwatch<FakeClock> b;
    b.start();
    FakeClock::advance(10ms);
    ZTH_EXPECT_EQ(b.elapsed().count(), 10LL);
    ZTH_EXPECT_EQ(a.elapsed().count(), 11LL);
}
ZTH_TEST("robust", "Stopwatch works with the real steady_clock too (it is only a template)") {
    Stopwatch<std::chrono::steady_clock> w;
    w.start();
    ZTH_EXPECT_TRUE(w.elapsed().count() >= 0);
    ZTH_EXPECT_TRUE(w.lap().count() >= 0);
}
ZTH_TEST("robust", "uniform_below rejects exactly the values at or above 2^32 - (2^32 % n)") {
    // n = 10: 2^32 % 10 == 6, so the limit is 4294967290: 4294967289 is accepted, 4294967290 and up are not
    ScriptedEngine accepted({4294967289u});
    ZTH_EXPECT_EQ(uniform_below(accepted, 10), 9u);
    ZTH_EXPECT_EQ(accepted.calls, 1);
    ScriptedEngine rejected({4294967290u, 4294967295u, 13u});
    ZTH_EXPECT_EQ(uniform_below(rejected, 10), 3u);
    ZTH_EXPECT_EQ(rejected.calls, 3);
}
ZTH_TEST("robust", "uniform_below for a power of two never rejects") {
    ScriptedEngine e({0xFFFFFFFFu});
    ZTH_EXPECT_EQ(uniform_below(e, 8), 7u);
    ZTH_EXPECT_EQ(e.calls, 1);
    ScriptedEngine one({123456u});
    ZTH_EXPECT_EQ(uniform_below(one, 1), 0u);
}
ZTH_TEST("robust", "uniform_below is roughly uniform: every value of a 7-sided die appears about equally often") {
    std::mt19937 rng(99);
    int counts[7] = {};
    for (int i = 0; i < 70000; ++i) ++counts[uniform_below(rng, 7)];
    for (int c : counts) {
        if (c < 9500 || c > 10500) {
            ::zth::fail("a value appeared " + std::to_string(c) + " times out of 70000 (expected about 10000)");
            return;
        }
    }
}
ZTH_TEST("robust", "the standard mt19937 gives the standard sequence (the 10000th output for the default seed is 4123659995)") {
    std::mt19937 rng;  // default seed 5489
    std::uint32_t last = 0;
    for (int i = 0; i < 10000; ++i) last = rng();
    ZTH_EXPECT_EQ(last, 4123659995u);
}
ZTH_TEST("robust", "simulate_rolls: different seeds give different rolls; 0 rolls give none; 1-sided dice always roll 1") {
    ZTH_EXPECT_FALSE(simulate_rolls(1, 100, 6) == simulate_rolls(2, 100, 6));
    ZTH_EXPECT_TRUE(simulate_rolls(1, 0, 6).empty());
    for (int r : simulate_rolls(3, 20, 1)) ZTH_EXPECT_EQ(r, 1);
}
ZTH_TEST("robust", "simulate_rolls produces every face of a die, with a reasonable distribution") {
    int counts[7] = {};
    for (int r : simulate_rolls(12345, 60000, 6)) ++counts[r];
    ZTH_EXPECT_EQ(counts[0], 0);
    for (int face = 1; face <= 6; ++face) {
        if (counts[face] < 9400 || counts[face] > 10600) {
            ::zth::fail("face " + std::to_string(face) + " came up " + std::to_string(counts[face]) + " times out of 60000");
            return;
        }
    }
}
ZTH_TEST("robust", "shuffle_deterministic matches the reference Fisher-Yates and handles tiny vectors") {
    V v{1, 2, 3, 4, 5, 6, 7};
    std::mt19937 r1(11), r2(11);
    shuffle_deterministic(v, r1);
    V expected{1, 2, 3, 4, 5, 6, 7};
    for (std::size_t i = expected.size(); i > 1; --i) {
        const std::size_t j = (r2() % 4294967296ull) % i;  // the same as uniform_below when nothing is rejected
        std::swap(expected[i - 1], expected[j]);
    }
    ZTH_EXPECT_TRUE(v == expected);
    V empty;
    V one{9};
    std::mt19937 r3(1);
    shuffle_deterministic(empty, r3);
    shuffle_deterministic(one, r3);
    ZTH_EXPECT_TRUE(empty.empty());
    ZTH_EXPECT_TRUE((one == V{9}));
}
ZTH_TEST("robust", "shuffle_deterministic uses every position: over many shuffles an element lands everywhere") {
    int landed[5] = {};
    for (std::uint32_t seed = 0; seed < 500; ++seed) {
        V v{0, 1, 2, 3, 4};
        std::mt19937 rng(seed);
        shuffle_deterministic(v, rng);
        ++landed[v[0]];
    }
    for (int c : landed) ZTH_EXPECT_TRUE(c > 50);
}
ZTH_TEST("robust", "prefix_sums do not overflow an int, and of nothing is nothing") {
    const auto big = prefix_sums({2000000000, 2000000000, 2000000000});
    ZTH_EXPECT_EQ(big.size(), std::size_t{3});
    if (big.size() == 3) ZTH_EXPECT_EQ(big[2], 6000000000LL);
    ZTH_EXPECT_TRUE(prefix_sums({}).empty());
    const auto neg = prefix_sums({5, -3, -4});
    ZTH_EXPECT_TRUE((neg == std::vector<long long>{5, 2, -2}));
}
ZTH_TEST("robust", "dot_product does not overflow an int") {
    ZTH_EXPECT_EQ(dot_product({100000, 100000}, {100000, 100000}), 20000000000LL);
    ZTH_EXPECT_EQ(dot_product({}, {}), 0LL);
    ZTH_EXPECT_EQ(dot_product({-3, 4}, {5, -6}), -39LL);
}
ZTH_TEST("robust", "gcd_of_all: empty, with a zero, with negatives, coprime") {
    ZTH_EXPECT_EQ(gcd_of_all({}), 0);
    ZTH_EXPECT_EQ(gcd_of_all({0, 9}), 9);
    ZTH_EXPECT_EQ(gcd_of_all({-12, 18}), 6);
    ZTH_EXPECT_EQ(gcd_of_all({9, 28}), 1);
    ZTH_EXPECT_EQ(gcd_of_all({100, 75, 50}), 25);
}
ZTH_TEST("robust", "nearly_equal: relative tolerance for big numbers, absolute for numbers near zero") {
    ZTH_EXPECT_TRUE(nearly_equal(1e10, 1e10 + 1.0));        // a relative difference of 1e-10
    ZTH_EXPECT_FALSE(nearly_equal(1e10, 1e10 + 100.0));     // 1e-8
    ZTH_EXPECT_FALSE(nearly_equal(1e-12, 0.0));             // no absolute tolerance: relative to 1e-12 it is a 100% difference
    ZTH_EXPECT_TRUE(nearly_equal(1e-12, 0.0, 1e-9, 1e-9));  // with an absolute tolerance it is fine
    ZTH_EXPECT_TRUE(nearly_equal(-1e-12, 1e-12, 1e-9, 1e-9));
}
ZTH_TEST("robust", "nearly_equal: infinities and NaN") {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    ZTH_EXPECT_TRUE(nearly_equal(inf, inf));
    ZTH_EXPECT_FALSE(nearly_equal(inf, -inf));
    ZTH_EXPECT_FALSE(nearly_equal(inf, 1e308));
    ZTH_EXPECT_FALSE(nearly_equal(nan, nan));
    ZTH_EXPECT_FALSE(nearly_equal(nan, 1.0));
}
ZTH_TEST("robust", "nearly_equal: the tolerance is relative to the LARGER of the two values") {
    ZTH_EXPECT_TRUE(nearly_equal(1.0, 1.6, 0.5));
    ZTH_EXPECT_TRUE(nearly_equal(1.6, 1.0, 0.5));
}
ZTH_TEST("robust", "nearly_equal is symmetric and works with negative numbers") {
    ZTH_EXPECT_TRUE(nearly_equal(-5.0, -5.0 - 1e-12));
    ZTH_EXPECT_EQ(nearly_equal(1.0, 1.0 + 1e-6, 1e-3), nearly_equal(1.0 + 1e-6, 1.0, 1e-3));
    ZTH_EXPECT_FALSE(nearly_equal(-1.0, 1.0));
}
ZTH_TEST("robust", "sum_accurate: a million 0.1 are 100000 to about 1e-9 (a plain loop is off by about 1e-6)") {
    const std::vector<double> tenths(1000000, 0.1);
    double plain = 0.0;
    for (double x : tenths) plain += x;
    ZTH_EXPECT_TRUE(std::fabs(plain - 100000.0) > 1e-8);  // the plain loop drifts: that is the point
    ZTH_EXPECT_NEAR(sum_accurate(tenths), 100000.0, 1e-9);
}
ZTH_TEST("robust", "sum_accurate keeps small values next to a huge one") {
    std::vector<double> v{1e16};
    for (int i = 0; i < 1000; ++i) v.push_back(1.0);
    v.push_back(-1e16);
    ZTH_EXPECT_NEAR(sum_accurate(v), 1000.0, 1.0);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "prefix_sums allocates its result once (not a push_back per element)") {
    const V v(10000, 1);
    zth::mem::Scope scope;
    const auto s = prefix_sums(v);
    ZTH_EXPECT_EQ(s.back(), 10000LL);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
}
ZTH_TEST("fast", "dot_product and gcd_of_all allocate nothing") {
    const V a(1000, 3), b(1000, 4);
    zth::mem::Scope scope;
    ZTH_EXPECT_EQ(dot_product(a, b), 12000LL);
    ZTH_EXPECT_EQ(gcd_of_all(a), 3);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "simulate_rolls allocates its result once") {
    zth::mem::Scope scope;
    const V r = simulate_rolls(1, 5000, 6);
    ZTH_EXPECT_EQ(r.size(), std::size_t{5000});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
}
ZTH_TEST("fast", "shuffling a million elements is linear (no insert in the middle)") {
    V v(1000000);
    for (int i = 0; i < 1000000; ++i) v[static_cast<std::size_t>(i)] = i;
    std::mt19937 rng(8);
    shuffle_deterministic(v, rng);
    long long sum = 0;
    for (int x : v) sum += x;
    ZTH_EXPECT_EQ(sum, 499999500000LL);
}
ZTH_TEST("fast", "a million rolls of a die") {
    const V r = simulate_rolls(77, 1000000, 6);
    long long sum = 0;
    for (int x : r) sum += x;
    ZTH_EXPECT_TRUE(sum > 3400000 && sum < 3600000);
}
