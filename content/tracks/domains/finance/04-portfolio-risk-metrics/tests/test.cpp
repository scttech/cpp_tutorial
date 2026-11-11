#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "risk.hpp"
#include "zth_test.hpp"

using namespace risk;

namespace {

double val(const std::optional<double>& o) { return o.value_or(-12345.0); }

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// two-pass reference (mean first, then squared deviations): accurate
double ref_stdev(const std::vector<double>& v) {
    double m = 0;
    for (double x : v) m += x;
    m /= static_cast<double>(v.size());
    double s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / static_cast<double>(v.size() - 1));
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "mean of a few numbers; nullopt for an empty vector") {
    ZTH_EXPECT_EQ(val(mean({1, 2, 3, 4})), 2.5);
    ZTH_EXPECT_EQ(val(mean({-3, 3})), 0.0);
    ZTH_EXPECT_EQ(val(mean({42})), 42.0);
    ZTH_EXPECT_FALSE(mean({}).has_value());
}
ZTH_TEST("works", "stdev is the SAMPLE standard deviation (n - 1)") {
    ZTH_EXPECT_NEAR(*stdev({2, 4, 4, 4, 5, 5, 7, 9}), std::sqrt(32.0 / 7), 1e-12);   // population would be exactly 2
    ZTH_EXPECT_NEAR(*stdev({1, 3}), std::sqrt(2.0), 1e-12);
    ZTH_EXPECT_EQ(val(stdev({5, 5, 5})), 0.0);
    ZTH_EXPECT_FALSE(stdev({1}).has_value());
    ZTH_EXPECT_FALSE(stdev({}).has_value());
}
ZTH_TEST("works", "summarize: returns, mean, stdev, best, worst, total return, drawdown") {
    const auto s = summarize(Series({100, 110, 99, 108.9}));
    ZTH_EXPECT_TRUE(s.has_value());
    ZTH_EXPECT_EQ(s->returns, std::size_t{3});
    ZTH_EXPECT_NEAR(s->mean, 0.1 / 3, 1e-12);
    ZTH_EXPECT_NEAR(s->stdev, 0.1154700538379252, 1e-12);
    ZTH_EXPECT_NEAR(s->best, 0.1, 1e-12);
    ZTH_EXPECT_NEAR(s->worst, -0.1, 1e-12);
    ZTH_EXPECT_NEAR(s->total_return, 0.089, 1e-12);
    ZTH_EXPECT_NEAR(s->max_drawdown, 0.1, 1e-12);   // from the peak 110 down to 99
}
ZTH_TEST("works", "max drawdown is measured from the highest peak so far, as a fraction of the peak") {
    ZTH_EXPECT_NEAR(summarize(Series({100, 80, 90, 60, 120, 100}))->max_drawdown, 0.4, 1e-12);         // 100 -> 60
    ZTH_EXPECT_NEAR(summarize(Series({50, 100, 75, 120, 60}))->max_drawdown, 0.5, 1e-12);              // 120 -> 60 beats 100 -> 75
    ZTH_EXPECT_NEAR(summarize(Series({10, 20, 30, 40}))->max_drawdown, 0.0, 0.0);                      // only up
    ZTH_EXPECT_NEAR(summarize(Series({100, 90, 80, 70}))->max_drawdown, 0.3, 1e-12);                   // only down
}
ZTH_TEST("works", "historical VaR is a positive loss: the (100 - c)% worst quantile of the returns") {
    std::vector<double> r;
    for (int i = 1; i <= 100; ++i) r.push_back((i - 51) / 100.0);   // -0.50 ... +0.49
    std::reverse(r.begin(), r.end());                                 // not sorted
    ZTH_EXPECT_NEAR(*historical_var(r, 95), 0.46, 1e-12);   // the 5th smallest: -0.46
    ZTH_EXPECT_NEAR(*historical_var(r, 99), 0.50, 1e-12);   // the 1st smallest
    ZTH_EXPECT_NEAR(*historical_var(r, 90), 0.41, 1e-12);   // the 10th smallest
    ZTH_EXPECT_NEAR(*historical_var(r, 50), 0.01, 1e-12);   // the 50th smallest: -0.01
}
ZTH_TEST("works", "VaR is 0 when the quantile is a gain") {
    ZTH_EXPECT_EQ(val(historical_var({0.01, 0.02, 0.03, 0.04}, 95)), 0.0);
    ZTH_EXPECT_EQ(val(historical_var({0.0, 0.0}, 90)), 0.0);
}
ZTH_TEST("works", "Sharpe ratio: (mean - risk free) / stdev, annualised with the square root of the periods") {
    const auto s = summarize(Series({100, 101, 103, 102, 105, 107}));
    ZTH_EXPECT_TRUE(s.has_value());
    const double expected = (s->mean - 0.001) / s->stdev * std::sqrt(252.0);
    ZTH_EXPECT_NEAR(*sharpe_ratio(*s, 0.001, 252), expected, 1e-9);
    ZTH_EXPECT_NEAR(*sharpe_ratio(*s, 0.0, 1), s->mean / s->stdev, 1e-12);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "stdev stays accurate for large numbers that are close together (no sum-of-squares formula)") {
    ZTH_EXPECT_NEAR(*stdev({1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16}), 5.477225575051661, 1e-6);
    ZTH_EXPECT_NEAR(*stdev({1e12 + 0.1, 1e12 + 0.2, 1e12 + 0.3}), 0.1, 1e-3);
    std::vector<double> v;
    for (int i = 0; i < 100000; ++i) v.push_back(1e8 + (i % 7));
    ZTH_EXPECT_NEAR(*stdev(v), ref_stdev(v), 1e-6);
    ZTH_EXPECT_NEAR(*mean(v), 1e8 + 2.9999, 1e-3);
}
ZTH_TEST("robust", "stdev of a constant series is exactly zero, also for large values") {
    ZTH_EXPECT_EQ(val(stdev({1e15, 1e15, 1e15, 1e15})), 0.0);
    ZTH_EXPECT_EQ(val(stdev(std::vector<double>(1000, 0.1))), 0.0);
}
ZTH_TEST("robust", "non-finite numbers are refused everywhere") {
    ZTH_EXPECT_FALSE(mean({1, kNaN}).has_value());
    ZTH_EXPECT_FALSE(mean({kInf}).has_value());
    ZTH_EXPECT_FALSE(stdev({1, 2, kNaN}).has_value());
    ZTH_EXPECT_FALSE(stdev({kInf, 1}).has_value());
    ZTH_EXPECT_FALSE(historical_var({0.1, kNaN, -0.2}, 95).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100, kNaN, 101})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100, 101, kInf})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({kNaN, 100, 101})).has_value());
}
ZTH_TEST("robust", "summarize refuses fewer than two prices and any price that is zero or negative") {
    ZTH_EXPECT_FALSE(summarize(Series()).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100, 0, 100})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100, -5, 100})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({0, 100})).has_value());
    ZTH_EXPECT_FALSE(summarize(Series({100, 101, 102, 0})).has_value());
    ZTH_EXPECT_TRUE(summarize(Series({100, 101})).has_value());
}
ZTH_TEST("robust", "a single return: stdev 0, best = worst = mean; a flat series has no return and no drawdown") {
    const auto s = summarize(Series({100, 125}));
    ZTH_EXPECT_EQ(s->returns, std::size_t{1});
    ZTH_EXPECT_EQ(s->stdev, 0.0);
    ZTH_EXPECT_NEAR(s->mean, 0.25, 1e-12);
    ZTH_EXPECT_NEAR(s->best, 0.25, 1e-12);
    ZTH_EXPECT_NEAR(s->worst, 0.25, 1e-12);
    const auto flat = summarize(Series({50, 50, 50, 50}));
    ZTH_EXPECT_EQ(flat->mean, 0.0);
    ZTH_EXPECT_EQ(flat->stdev, 0.0);
    ZTH_EXPECT_EQ(flat->max_drawdown, 0.0);
    ZTH_EXPECT_EQ(flat->total_return, 0.0);
    ZTH_EXPECT_EQ(flat->best, 0.0);
    ZTH_EXPECT_EQ(flat->worst, 0.0);
}
ZTH_TEST("robust", "best and worst when all returns are negative or all positive (initial values must not be 0)") {
    const auto down = summarize(Series({100, 90, 81, 80}));
    ZTH_EXPECT_NEAR(down->best, 80.0 / 81 - 1, 1e-12);
    ZTH_EXPECT_NEAR(down->worst, -0.1, 1e-12);
    const auto up = summarize(Series({100, 101, 103.02, 106.1106}));
    ZTH_EXPECT_NEAR(up->worst, 0.01, 1e-9);
    ZTH_EXPECT_NEAR(up->best, 0.03, 1e-9);
}
ZTH_TEST("robust", "a crash of 99.9 percent and a recovery: drawdown near 1, total return exact") {
    const auto s = summarize(Series({1000, 1, 1000}));
    ZTH_EXPECT_NEAR(s->max_drawdown, 0.999, 1e-12);
    ZTH_EXPECT_NEAR(s->total_return, 0.0, 1e-12);
    ZTH_EXPECT_NEAR(s->worst, -0.999, 1e-12);
}
ZTH_TEST("robust", "VaR index is computed with integers: 100 returns at 95 percent is the 5th smallest, not the 6th") {
    std::vector<double> r;
    for (int i = 1; i <= 100; ++i) r.push_back(-i / 1000.0);   // -0.001 ... -0.100, VaR at 95 = the 5th smallest = -0.096
    ZTH_EXPECT_NEAR(*historical_var(r, 95), 0.096, 1e-12);
    std::vector<double> r20;
    for (int i = 1; i <= 20; ++i) r20.push_back(-i / 100.0);
    ZTH_EXPECT_NEAR(*historical_var(r20, 95), 0.20, 1e-12);   // 20 * 5 / 100 = 1: the smallest
    ZTH_EXPECT_NEAR(*historical_var(r20, 90), 0.19, 1e-12);   // 2nd smallest
    ZTH_EXPECT_NEAR(*historical_var(r20, 99), 0.20, 1e-12);   // ceil(0.2) = 1: the smallest
    std::vector<double> r7 = {-0.07, -0.01, 0.02, -0.04, 0.03, -0.05, 0.01};
    ZTH_EXPECT_NEAR(*historical_var(r7, 95), 0.07, 1e-12);   // ceil(0.35) = 1
    ZTH_EXPECT_NEAR(*historical_var(r7, 50), 0.01, 1e-12);   // ceil(3.5) = 4: the 4th smallest (sorted -0.07 -0.05 -0.04 -0.01 ...)
}
ZTH_TEST("robust", "VaR validation: confidence outside 1..99, no returns, a single return") {
    ZTH_EXPECT_FALSE(historical_var({0.1, -0.1}, 0).has_value());
    ZTH_EXPECT_FALSE(historical_var({0.1, -0.1}, 100).has_value());
    ZTH_EXPECT_FALSE(historical_var({0.1, -0.1}, -5).has_value());
    ZTH_EXPECT_FALSE(historical_var({0.1, -0.1}, 150).has_value());
    ZTH_EXPECT_FALSE(historical_var({}, 95).has_value());
    ZTH_EXPECT_NEAR(*historical_var({-0.03}, 95), 0.03, 1e-12);
    ZTH_EXPECT_NEAR(*historical_var({-0.03, -0.03, -0.03}, 1), 0.03, 1e-12);
}
ZTH_TEST("robust", "VaR leaves the caller's vector alone (it takes a copy) and handles duplicates") {
    const std::vector<double> r = {0.05, -0.02, -0.02, 0.01, -0.09, -0.02};
    const std::vector<double> before = r;
    ZTH_EXPECT_NEAR(*historical_var(r, 80), 0.02, 1e-12);   // ceil(6*20/100)=2 -> the 2nd smallest = -0.02
    ZTH_EXPECT_TRUE(r == before);
}
ZTH_TEST("robust", "Sharpe ratio is undefined for a constant series and for bad annualisation; negative for a losing one") {
    const auto flat = summarize(Series({10, 10, 10}));
    ZTH_EXPECT_FALSE(sharpe_ratio(*flat, 0.0, 252).has_value());
    const auto up = summarize(Series({100, 101, 103, 102, 105}));
    ZTH_EXPECT_FALSE(sharpe_ratio(*up, 0.0, 0).has_value());
    ZTH_EXPECT_FALSE(sharpe_ratio(*up, 0.0, -4).has_value());
    const auto down = summarize(Series({100, 98, 99, 95, 96, 90}));
    ZTH_EXPECT_TRUE(*sharpe_ratio(*down, 0.0, 252) < 0);
    ZTH_EXPECT_TRUE(*sharpe_ratio(*up, 0.01, 12) < *sharpe_ratio(*up, 0.0, 12));   // a higher risk-free rate lowers it
}
ZTH_TEST_TIMEOUT("robust", "random price paths: summarize agrees with a direct computation of every figure", 10) {
    ::zth::Lcg rng(77);
    for (int t = 0; t < 200; ++t) {
        std::vector<double> p = {static_cast<double>(50 + rng.below(100))};
        const int n = 2 + rng.below(60);
        for (int i = 1; i < n; ++i) p.push_back(p.back() * (0.9 + rng.below(2001) / 10000.0));
        const auto s = summarize(Series(p));
        ZTH_EXPECT_TRUE(s.has_value());
        std::vector<double> r;
        for (std::size_t i = 1; i < p.size(); ++i) r.push_back(p[i] / p[i - 1] - 1);
        ZTH_EXPECT_NEAR(s->mean, *mean(r), 1e-12);
        if (r.size() >= 2) ZTH_EXPECT_NEAR(s->stdev, ref_stdev(r), 1e-12);
        ZTH_EXPECT_NEAR(s->best, *std::max_element(r.begin(), r.end()), 1e-12);
        ZTH_EXPECT_NEAR(s->worst, *std::min_element(r.begin(), r.end()), 1e-12);
        ZTH_EXPECT_NEAR(s->total_return, p.back() / p.front() - 1, 1e-12);
        double peak = p[0], dd = 0;
        for (double x : p) {
            peak = std::max(peak, x);
            dd = std::max(dd, (peak - x) / peak);
        }
        ZTH_EXPECT_NEAR(s->max_drawdown, dd, 1e-12);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "summarize reads every price once: one pass (at most 2 reads per price on 200000 prices)", 10) {
    std::vector<double> p;
    ::zth::Lcg rng(3);
    double price = 100;
    for (int i = 0; i < 200000; ++i) {
        price *= 0.999 + rng.below(2001) / 1000000.0;
        p.push_back(price);
    }
    const Series series(p);
    stats::reset();
    const auto s = summarize(series);
    ZTH_EXPECT_TRUE(s.has_value());
    ZTH_EXPECT_TRUE(stats::reads <= 2ULL * 200000);
}
ZTH_TEST_TIMEOUT("fast", "summarize allocates nothing", 10) {
    std::vector<double> p(100000);
    for (std::size_t i = 0; i < p.size(); ++i) p[i] = 100 + static_cast<double>(i % 50);
    const Series series(p);
    zth::mem::Scope scope;
    const auto s = summarize(series);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(s.has_value());
}
ZTH_TEST_TIMEOUT("fast", "mean and stdev make no allocation; historical_var makes one (the copy)", 10) {
    std::vector<double> v(100000);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<double>(i % 100) / 1000.0 - 0.05;
    {
        zth::mem::Scope scope;
        (void)mean(v);
        (void)stdev(v);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    }
    {
        zth::mem::Scope scope;
        (void)historical_var(v, 95);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    }
}
ZTH_TEST_TIMEOUT("fast", "5 million returns: VaR (a quantile, not a full sort) and summarize in time", 10) {
    std::vector<double> v(5000000);
    ::zth::Lcg rng(1);
    for (double& x : v) x = (rng.below(20001) - 10000) / 100000.0;
    ZTH_EXPECT_TRUE(historical_var(v, 99).has_value());
    ZTH_EXPECT_TRUE(stdev(v).has_value());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("risk.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("risk.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "float"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "variance is not computed from sums of squares: no pow, no accumulate over squares, no sum_sq style helpers") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("risk.hpp"));
    for (const char* word : {"pow", "inner_product", "transform_reduce", "accumulate", "reduce"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("risk.hpp uses `") + word + "`: update the mean and the squared deviations one value at a time (Welford)");
            return;
        }
    }
}
ZTH_TEST("clean", "invalid input is std::optional, validated with std::isfinite; the quantile does not sort everything (nth_element or a selection)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("risk.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "optional").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "isfinite").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sort").empty());
}
