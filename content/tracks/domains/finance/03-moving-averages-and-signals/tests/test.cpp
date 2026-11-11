#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "averages.hpp"
#include "zth_test.hpp"

using namespace ma;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// the average of the last w values, summed from scratch
double reference_sma(const std::vector<double>& v, std::size_t w) {
    double sum = 0;
    for (std::size_t i = v.size() - w; i < v.size(); ++i) sum += v[i];
    return sum / static_cast<double>(w);
}

std::string signals(Crossover& c, const std::vector<double>& prices) {
    std::string out;
    for (double p : prices) {
        const Signal s = c.push(p);
        out += s == Signal::Buy ? 'B' : s == Signal::Sell ? 'S' : '.';
    }
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "Sma: nothing until the window is full, then the average of the last `window` prices") {
    Sma sma(3);
    ZTH_EXPECT_FALSE(sma.push(10).has_value());
    ZTH_EXPECT_FALSE(sma.push(20).has_value());
    ZTH_EXPECT_FALSE(sma.ready());
    ZTH_EXPECT_EQ(sma.count(), std::size_t{2});
    ZTH_EXPECT_EQ(*sma.push(30), 20.0);
    ZTH_EXPECT_TRUE(sma.ready());
    ZTH_EXPECT_EQ(*sma.push(40), 30.0);   // 20 30 40
    ZTH_EXPECT_EQ(*sma.push(70), 140.0 / 3);   // 30 40 70
    ZTH_EXPECT_EQ(*sma.value(), 140.0 / 3);
    ZTH_EXPECT_EQ(sma.count(), std::size_t{3});
}
ZTH_TEST("works", "Sma with window 1 is the price itself; window 0 counts as 1") {
    Sma one(1), zero(0);
    for (double p : {5.0, -2.5, 9.0}) {
        ZTH_EXPECT_EQ(*one.push(p), p);
        ZTH_EXPECT_EQ(*zero.push(p), p);
    }
}
ZTH_TEST("works", "Sma over a long ramp agrees with the sum recomputed from scratch") {
    Sma sma(7);
    std::vector<double> prices;
    for (int i = 0; i < 100; ++i) {
        prices.push_back(100 + i * 0.5 + (i % 3));
        const auto v = sma.push(prices.back());
        if (i >= 6) ZTH_EXPECT_NEAR(*v, reference_sma(prices, 7), 1e-9);
        else ZTH_EXPECT_FALSE(v.has_value());
    }
}
ZTH_TEST("works", "Ema: the first price seeds it, then alpha = 2 / (period + 1)") {
    Ema ema(3);   // alpha 0.5
    ZTH_EXPECT_FALSE(ema.ready());
    ZTH_EXPECT_FALSE(ema.value().has_value());
    ZTH_EXPECT_EQ(ema.push(10), 10.0);
    ZTH_EXPECT_TRUE(ema.ready());
    ZTH_EXPECT_EQ(ema.push(20), 15.0);
    ZTH_EXPECT_EQ(ema.push(30), 22.5);
    ZTH_EXPECT_EQ(*ema.value(), 22.5);
    Ema nine(9);  // alpha 0.2
    nine.push(100);
    ZTH_EXPECT_NEAR(nine.push(110), 102.0, 1e-12);
    ZTH_EXPECT_NEAR(nine.push(90), 99.6, 1e-12);
}
ZTH_TEST("works", "Ema with period 1 follows the price; period 0 counts as 1") {
    Ema a(1), b(0);
    for (double p : {3.0, 8.0, -1.0}) {
        ZTH_EXPECT_EQ(a.push(p), p);
        ZTH_EXPECT_EQ(b.push(p), p);
    }
}
ZTH_TEST("works", "Crossover: one Buy when the fast average rises above the slow one, one Sell when it falls below") {
    Crossover c(2, 4);
    // prices fall, then rise (fast above slow: Buy), then fall (Sell)
    const std::vector<double> prices = {10, 9, 8, 7, 6, 7, 9, 12, 14, 13, 10, 7, 5};
    const std::string s = signals(c, prices);
    ZTH_EXPECT_EQ(s.size(), prices.size());
    ZTH_EXPECT_EQ(std::count(s.begin(), s.end(), 'B'), 1);
    ZTH_EXPECT_EQ(std::count(s.begin(), s.end(), 'S'), 1);
    ZTH_EXPECT_TRUE(s.find('B') < s.find('S'));
}
ZTH_TEST("works", "Crossover gives no signal before both averages are ready, and none on the first difference") {
    Crossover c(2, 4);
    ZTH_EXPECT_EQ(signals(c, {1, 2, 3}), std::string("..."));
    ZTH_EXPECT_EQ(signals(c, {4}), std::string("."));   // the slow average becomes ready: the starting side is set, no signal
}
ZTH_TEST("works", "reset() forgets everything") {
    Sma sma(2);
    sma.push(5);
    sma.push(7);
    sma.reset();
    ZTH_EXPECT_FALSE(sma.ready());
    ZTH_EXPECT_EQ(sma.count(), std::size_t{0});
    ZTH_EXPECT_FALSE(sma.push(100).has_value());
    ZTH_EXPECT_EQ(*sma.push(200), 150.0);
    Ema ema(5);
    ema.push(5);
    ema.reset();
    ZTH_EXPECT_FALSE(ema.ready());
    ZTH_EXPECT_EQ(ema.push(42), 42.0);
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "the running sum does not drift: 200000 big prices mixed with small ones, the final average is still exact", 10) {
    Sma sma(10);
    for (int i = 0; i < 200000; ++i) {
        sma.push(1e12);
        sma.push(0.1);
    }
    std::optional<double> last;
    for (int i = 0; i < 10; ++i) last = sma.push(0.1);
    ZTH_EXPECT_NEAR(*last, 0.1, 1e-9);
}
ZTH_TEST_TIMEOUT("robust", "random walk, 300000 prices: the O(1) average stays within 1e-9 of the sum recomputed from scratch", 15) {
    ::zth::Lcg rng(31337);
    Sma sma(50);
    std::vector<double> tail;
    double price = 1000;
    for (int i = 0; i < 300000; ++i) {
        price += (rng.below(2001) - 1000) / 100.0;
        const auto v = sma.push(price);
        tail.push_back(price);
        if (tail.size() > 50) tail.erase(tail.begin());
        if (i % 20000 == 19999) {
            ZTH_EXPECT_TRUE(v.has_value());
            ZTH_EXPECT_NEAR(*v, reference_sma(tail, 50), 1e-9 * (1 + std::fabs(price)));
        }
    }
}
ZTH_TEST("robust", "NaN and infinity are ignored: nothing changes") {
    Sma sma(3);
    sma.push(1);
    sma.push(2);
    ZTH_EXPECT_FALSE(sma.push(kNaN).has_value());
    ZTH_EXPECT_FALSE(sma.push(kInf).has_value());
    ZTH_EXPECT_EQ(sma.count(), std::size_t{2});
    ZTH_EXPECT_EQ(*sma.push(6), 3.0);
    ZTH_EXPECT_EQ(*sma.push(-kInf), 3.0);   // returns the current value
    ZTH_EXPECT_EQ(*sma.push(kNaN), 3.0);
    ZTH_EXPECT_EQ(*sma.push(9), (2.0 + 6 + 9) / 3);
    Ema ema(3);
    ZTH_EXPECT_EQ(ema.push(kNaN), 0.0);
    ZTH_EXPECT_FALSE(ema.ready());
    ema.push(10);
    ZTH_EXPECT_EQ(ema.push(kInf), 10.0);
    ZTH_EXPECT_EQ(ema.push(20), 15.0);
    Crossover c(2, 3);
    ZTH_EXPECT_TRUE(c.push(kNaN) == Signal::None);
    ZTH_EXPECT_EQ(signals(c, {1, 2, 3}), std::string("..."));
}
ZTH_TEST("robust", "Sma with a huge window, and a window longer than the data") {
    Sma big(1000000);
    for (int i = 0; i < 999999; ++i) ZTH_EXPECT_FALSE(big.push(1.0).has_value());
    ZTH_EXPECT_EQ(*big.push(1.0), 1.0);
    ZTH_EXPECT_EQ(*big.push(3.0), 1.000002);
}
ZTH_TEST("robust", "Ema converges to a constant price and stays between its inputs") {
    Ema ema(10);
    ema.push(0);
    for (int i = 0; i < 400; ++i) ema.push(50);
    ZTH_EXPECT_NEAR(*ema.value(), 50.0, 1e-9);
    Ema e2(4);
    ::zth::Lcg rng(1);
    e2.push(10);
    for (int i = 0; i < 1000; ++i) {
        const double v = e2.push(10 + rng.below(100));
        ZTH_EXPECT_TRUE(v >= 10 && v <= 110);
    }
}
ZTH_TEST("robust", "Crossover: a difference of exactly zero is neutral, and the rule looks at the previous difference only") {
    Crossover c(1, 2);   // fast = the price, slow = the mean of the last two: diff = (price - previous price) / 2
    ZTH_EXPECT_EQ(signals(c, {5, 4, 4, 5}), std::string("...B"));      // diff -0.5, 0, +0.5: one Buy, at the +
    Crossover d(1, 2);
    ZTH_EXPECT_EQ(signals(d, {5, 6, 6, 6, 7}), std::string("....B")); // +0.5, 0, 0, +0.5: from 0 to + is a Buy (touch and go)
    Crossover e(1, 2);
    ZTH_EXPECT_EQ(signals(e, {5, 6, 5, 4, 5}), std::string("..S.B"));  // +, -, -, +: Sell then Buy, nothing in between
}
ZTH_TEST("robust", "Crossover does not repeat a signal while the fast average stays on one side") {
    Crossover c(2, 5);
    std::vector<double> prices;
    for (int i = 0; i < 40; ++i) prices.push_back(100 - i);          // falling
    for (int i = 0; i < 60; ++i) prices.push_back(60 + i * 2);       // rising
    const std::string s = signals(c, prices);
    ZTH_EXPECT_EQ(std::count(s.begin(), s.end(), 'B'), 1);
    ZTH_EXPECT_EQ(std::count(s.begin(), s.end(), 'S'), 0);
}
ZTH_TEST("robust", "Crossover on a sine wave: Buys and Sells alternate and match the sign changes of the two averages") {
    Crossover c(3, 8);
    Sma f(3), s(8);
    char last = ' ';
    int buys = 0, sells = 0;
    double prev = 0;
    bool have_prev = false;
    for (int i = 0; i < 400; ++i) {
        const double p = 100 + 20 * std::sin(i * 0.15);
        const Signal sig = c.push(p);
        const auto fv = f.push(p);
        const auto sv = s.push(p);
        if (fv && sv) {
            const double diff = *fv - *sv;
            if (have_prev && prev <= 0 && diff > 0) ZTH_EXPECT_TRUE(sig == Signal::Buy);
            else if (have_prev && prev >= 0 && diff < 0) ZTH_EXPECT_TRUE(sig == Signal::Sell);
            else ZTH_EXPECT_TRUE(sig == Signal::None);
            prev = diff;
            have_prev = true;
        } else {
            ZTH_EXPECT_TRUE(sig == Signal::None);
        }
        if (sig == Signal::Buy) {
            ZTH_EXPECT_TRUE(last != 'B');
            last = 'B';
            ++buys;
        } else if (sig == Signal::Sell) {
            ZTH_EXPECT_TRUE(last != 'S');
            last = 'S';
            ++sells;
        }
    }
    ZTH_EXPECT_TRUE(buys >= 4 && sells >= 4);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "an update is O(1): 2 million prices through a window of one million in time", 10) {
    Sma sma(1000000);
    double last = 0;
    for (int i = 0; i < 2000000; ++i) {
        const auto v = sma.push(static_cast<double>(i % 1000));
        if (v) last = *v;
    }
    ZTH_EXPECT_TRUE(last > 400 && last < 600);
}
ZTH_TEST_TIMEOUT("fast", "no allocation after construction: 3 million pushes through Sma, Ema and Crossover", 10) {
    Sma sma(100);
    Ema ema(20);
    Crossover cross(10, 50);
    double acc = 0;
    zth::mem::Scope scope;
    for (int i = 0; i < 1000000; ++i) {
        const double p = 100 + (i % 17) * 0.5;
        const auto v = sma.push(p);
        acc += v ? *v : 0;
        acc += ema.push(p);
        acc += cross.push(p) == Signal::Buy ? 1 : 0;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(acc > 0);
}
ZTH_TEST_TIMEOUT("fast", "memory is bounded by the window, not by the number of prices", 10) {
    Sma sma(64);
    zth::mem::Scope scope;
    for (int i = 0; i < 2000000; ++i) sma.push(1.0 * i);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sizeof(Sma) < 200);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no float") {
    ZTH_EXPECT_NO_RAW_MEMORY("averages.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("averages.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "float"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "constant time means no loop over the window in push: no std::accumulate, no std::deque / std::list / erase from the front") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("averages.hpp"));
    for (const char* word : {"accumulate", "reduce", "deque", "list", "queue", "erase", "insert", "sort"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("averages.hpp uses `") + word + "`");
            return;
        }
    }
}
ZTH_TEST("clean", "bad input is checked with std::isfinite, not by comparing with itself") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("averages.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "isfinite").empty());
}
