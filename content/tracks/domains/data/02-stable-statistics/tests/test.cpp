#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "stats.hpp"
#include "zth_test.hpp"

namespace {

double val(const std::optional<double>& o) { return o.value_or(std::numeric_limits<double>::quiet_NaN()); }
const double kInf = std::numeric_limits<double>::infinity();
const double kNan = std::numeric_limits<double>::quiet_NaN();

// independent two-pass reference in long double
double ref_var(const std::vector<double>& v) {
    long double m = 0;
    for (double x : v) m += x;
    m /= static_cast<long double>(v.size());
    long double s = 0;
    for (double x : v) s += (static_cast<long double>(x) - m) * (static_cast<long double>(x) - m);
    return static_cast<double>(s / static_cast<long double>(v.size() - 1));
}
stats::Running of(const std::vector<double>& v) {
    stats::Running r;
    for (double x : v) r.add(x);
    return r;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "mean, variance, standard deviation, min and max of a small sample") {
    const stats::Running r = of({2, 4, 4, 4, 5, 5, 7, 9});
    ZTH_EXPECT_EQ(r.count(), std::size_t{8});
    ZTH_EXPECT_NEAR(val(r.mean()), 5.0, 1e-12);
    ZTH_EXPECT_NEAR(val(r.population_variance()), 4.0, 1e-12);
    ZTH_EXPECT_NEAR(val(r.variance()), 32.0 / 7.0, 1e-12);
    ZTH_EXPECT_NEAR(val(r.stddev()), std::sqrt(32.0 / 7.0), 1e-12);
    ZTH_EXPECT_NEAR(val(r.min()), 2.0, 0);
    ZTH_EXPECT_NEAR(val(r.max()), 9.0, 0);
}
ZTH_TEST("works", "an empty and a one-element Running: what is defined and what is not") {
    stats::Running r;
    ZTH_EXPECT_FALSE(r.mean().has_value());
    ZTH_EXPECT_FALSE(r.variance().has_value());
    ZTH_EXPECT_FALSE(r.population_variance().has_value());
    ZTH_EXPECT_FALSE(r.stddev().has_value());
    ZTH_EXPECT_FALSE(r.min().has_value());
    ZTH_EXPECT_FALSE(r.max().has_value());
    ZTH_EXPECT_TRUE(r.add(-3.5));
    ZTH_EXPECT_NEAR(val(r.mean()), -3.5, 0);
    ZTH_EXPECT_NEAR(val(r.min()), -3.5, 0);
    ZTH_EXPECT_NEAR(val(r.max()), -3.5, 0);
    ZTH_EXPECT_NEAR(val(r.population_variance()), 0.0, 0);
    ZTH_EXPECT_FALSE(r.variance().has_value());   // one value has no sample variance
    ZTH_EXPECT_FALSE(r.stddev().has_value());
}
ZTH_TEST("works", "min and max start from the first value, also when all values are negative or all are large") {
    const stats::Running neg = of({-5, -9, -1});
    ZTH_EXPECT_NEAR(val(neg.max()), -1.0, 0);
    ZTH_EXPECT_NEAR(val(neg.min()), -9.0, 0);
    const stats::Running big = of({1e300, 2e300});
    ZTH_EXPECT_NEAR(val(big.min()), 1e300, 0);
    ZTH_EXPECT_NEAR(val(big.max()), 2e300, 0);
}
ZTH_TEST("works", "NaN and infinite readings are refused and change nothing") {
    stats::Running r = of({1, 2, 3});
    for (double bad : {kNan, kInf, -kInf}) {
        ZTH_EXPECT_FALSE(r.add(bad));
        ZTH_EXPECT_EQ(r.count(), std::size_t{3});
        ZTH_EXPECT_NEAR(val(r.mean()), 2.0, 0);
        ZTH_EXPECT_NEAR(val(r.max()), 3.0, 0);
        ZTH_EXPECT_NEAR(val(r.variance()), 1.0, 1e-15);
    }
    stats::RunningCov c;
    ZTH_EXPECT_TRUE(c.add(1, 2));
    ZTH_EXPECT_FALSE(c.add(kNan, 1));
    ZTH_EXPECT_FALSE(c.add(1, kInf));
    ZTH_EXPECT_EQ(c.count(), std::size_t{1});
}
ZTH_TEST("works", "the compensated sum sees what the plain sum loses") {
    stats::CompensatedSum s;
    s.add(1e16);
    s.add(1.0);
    s.add(-1e16);
    ZTH_EXPECT_NEAR(s.value(), 1.0, 0);   // plain: 0
    stats::CompensatedSum t;
    t.add(1.0);   // an addend larger than the running sum
    t.add(1e100);
    t.add(1.0);
    t.add(-1e100);
    ZTH_EXPECT_NEAR(t.value(), 2.0, 0);   // plain and classic Kahan: 0
    stats::CompensatedSum million;
    double plain = 0;
    for (int i = 0; i < 1000000; ++i) {
        million.add(0.1);
        plain += 0.1;
    }
    ZTH_EXPECT_NEAR(million.value(), 100000.0, 1e-9);
    ZTH_EXPECT_TRUE(std::fabs(plain - 100000.0) > 1e-7);   // the test would be pointless otherwise
    ZTH_EXPECT_NEAR(stats::CompensatedSum().value(), 0.0, 0);
}
ZTH_TEST("works", "covariance and correlation of simple series") {
    stats::RunningCov c;
    ZTH_EXPECT_FALSE(c.correlation().has_value());
    for (int i = 1; i <= 5; ++i) c.add(i, 2.0 * i + 1.0);
    ZTH_EXPECT_NEAR(val(c.covariance()), 5.0, 1e-12);   // 2 * var(1..5) = 2 * 2.5
    ZTH_EXPECT_NEAR(val(c.correlation()), 1.0, 1e-12);
    stats::RunningCov d;
    for (int i = 1; i <= 5; ++i) d.add(i, 10.0 - i);
    ZTH_EXPECT_NEAR(val(d.correlation()), -1.0, 1e-12);
    stats::RunningCov e;   // a constant series has no correlation with anything
    for (int i = 1; i <= 5; ++i) e.add(i, 7.0);
    ZTH_EXPECT_FALSE(e.correlation().has_value());
    ZTH_EXPECT_NEAR(val(e.covariance()), 0.0, 1e-15);
    stats::RunningCov one;
    one.add(1, 2);
    ZTH_EXPECT_FALSE(one.covariance().has_value());
    const std::vector<double> xs = {1, 2, 3, 4, 5, 6}, ys = {2, 1, 4, 3, 7, 5};
    stats::RunningCov f;
    for (std::size_t i = 0; i < xs.size(); ++i) f.add(xs[i], ys[i]);
    ZTH_EXPECT_NEAR(val(f.correlation()), 0.7917946548886297, 1e-12);
}
ZTH_TEST("works", "percentiles by linear interpolation, and the median of an odd and an even count") {
    const std::vector<double> d = {15, 20, 35, 40, 50};
    ZTH_EXPECT_NEAR(val(stats::percentile(d, 0)), 15.0, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(d, 25)), 20.0, 1e-12);
    ZTH_EXPECT_NEAR(val(stats::percentile(d, 40)), 29.0, 1e-12);   // rank 1.6 -> 20 + 0.6 * 15
    ZTH_EXPECT_NEAR(val(stats::percentile(d, 90)), 46.0, 1e-12);   // rank 3.6 -> 40 + 0.6 * 10
    ZTH_EXPECT_NEAR(val(stats::percentile(d, 100)), 50.0, 0);
    ZTH_EXPECT_NEAR(val(stats::median(d)), 35.0, 0);
    const std::vector<double> even = {4, 1, 3, 2};   // unsorted on purpose
    ZTH_EXPECT_NEAR(val(stats::median(even)), 2.5, 1e-12);
    ZTH_EXPECT_NEAR(val(stats::percentile(even, 100)), 4.0, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(even, 0)), 1.0, 0);
    const std::vector<double> one = {42};
    ZTH_EXPECT_NEAR(val(stats::percentile(one, 73)), 42.0, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(one, 100)), 42.0, 0);
}
ZTH_TEST("works", "percentile refuses what has no answer, and does not reorder the caller's data") {
    const std::vector<double> d = {5, 1, 4};
    ZTH_EXPECT_FALSE(stats::percentile(std::vector<double>{}, 50).has_value());
    ZTH_EXPECT_FALSE(stats::percentile(d, -0.001).has_value());
    ZTH_EXPECT_FALSE(stats::percentile(d, 100.001).has_value());
    ZTH_EXPECT_FALSE(stats::percentile(d, kNan).has_value());
    ZTH_EXPECT_FALSE(stats::percentile(d, kInf).has_value());
    ZTH_EXPECT_FALSE(stats::percentile(std::vector<double>{1, kNan, 3}, 50).has_value());
    ZTH_EXPECT_FALSE(stats::median(std::vector<double>{}).has_value());
    ZTH_EXPECT_TRUE(d[0] == 5 && d[1] == 1 && d[2] == 4);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a huge common offset: the textbook formula loses every digit, Welford keeps them") {
    ZTH_EXPECT_NEAR(val(of({1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16}).variance()), 30.0, 1e-6);
    ZTH_EXPECT_NEAR(val(of({1e12 + 0.5, 1e12 + 1.5, 1e12 + 2.5, 1e12 + 3.5}).variance()), 5.0 / 3.0, 1e-3);
    zth::Lcg rng(9);
    std::vector<double> v;
    for (int i = 0; i < 5000; ++i) v.push_back(1e8 + rng.below(1000) * 0.001);
    const stats::Running r = of(v);
    ZTH_EXPECT_NEAR(val(r.variance()), ref_var(v), ref_var(v) * 1e-6);
    ZTH_EXPECT_NEAR(val(r.mean()), 1e8 + 0.5, 0.05);
    ZTH_EXPECT_TRUE(val(r.variance()) >= 0);   // the formula with cancellation goes negative
}
ZTH_TEST("robust", "the variance is never negative, also for identical values") {
    for (double x : {0.1, 1e-300, 1e8 + 0.1, 3.0}) {
        stats::Running r;
        for (int i = 0; i < 1000; ++i) r.add(x);
        ZTH_EXPECT_TRUE(val(r.variance()) >= 0.0);
        ZTH_EXPECT_NEAR(val(r.variance()), 0.0, 1e-20 + x * x * 1e-28);
    }
}
ZTH_TEST("robust", "merging partial results equals one pass over everything, for any split, including empty parts") {
    zth::Lcg rng(77);
    for (int round = 0; round < 200; ++round) {
        std::vector<double> v;
        const int n = 1 + rng.below(200);
        const double offset = rng.below(3) == 0 ? 1e7 : 0.0;
        for (int i = 0; i < n; ++i) v.push_back(offset + (rng.below(20001) - 10000) / 100.0);
        const stats::Running whole = of(v);
        stats::Running merged;
        std::size_t pos = 0;
        while (pos < v.size()) {
            const std::size_t len = static_cast<std::size_t>(rng.below(40));
            stats::Running part;
            for (std::size_t i = pos; i < std::min(v.size(), pos + len); ++i) part.add(v[i]);
            pos += len;
            merged.merge(part);
            merged.merge(stats::Running());   // merging nothing must change nothing
        }
        ZTH_EXPECT_EQ(merged.count(), whole.count());
        ZTH_EXPECT_NEAR(val(merged.mean()), val(whole.mean()), std::fabs(val(whole.mean())) * 1e-12 + 1e-12);
        if (n >= 2) ZTH_EXPECT_NEAR(val(merged.variance()), val(whole.variance()), val(whole.variance()) * 1e-8 + 1e-10);
        ZTH_EXPECT_NEAR(val(merged.min()), val(whole.min()), 0);
        ZTH_EXPECT_NEAR(val(merged.max()), val(whole.max()), 0);
    }
    stats::Running left;
    stats::Running right = of({1, 2, 3});
    left.merge(right);   // into an empty one
    ZTH_EXPECT_EQ(left.count(), std::size_t{3});
    ZTH_EXPECT_NEAR(val(left.variance()), 1.0, 1e-15);
    ZTH_EXPECT_NEAR(val(left.min()), 1.0, 0);
    stats::Running two = of({10, 20});
    two.merge(of({30}));   // a single value merged in
    ZTH_EXPECT_NEAR(val(two.variance()), 100.0, 1e-12);
}
ZTH_TEST("robust", "merging gives the two sides' means their correct weights (the unequal case)") {
    stats::Running a = of({0, 0, 0, 0, 0, 0, 0, 0, 0});   // nine zeros
    a.merge(of({10}));
    ZTH_EXPECT_NEAR(val(a.mean()), 1.0, 1e-15);
    ZTH_EXPECT_NEAR(val(a.population_variance()), 9.0, 1e-12);   // 9 * 1 + 81 over 10
    stats::Running b = of({10});
    b.merge(of({0, 0, 0, 0, 0, 0, 0, 0, 0}));
    ZTH_EXPECT_NEAR(val(b.mean()), 1.0, 1e-15);
    ZTH_EXPECT_NEAR(val(b.population_variance()), 9.0, 1e-12);
}
ZTH_TEST("robust", "Compensated summation matches a long-double reference on ill-conditioned data") {
    zth::Lcg rng(3);
    for (int round = 0; round < 50; ++round) {
        std::vector<double> v;
        long double exact = 0;
        for (int i = 0; i < 400; ++i) {
            const double x = (rng.below(2) ? 1.0 : -1.0) * (rng.below(3) == 0 ? 1e12 : 1.0) * (1 + rng.below(1000) / 997.0);
            v.push_back(x);
            exact += static_cast<long double>(x);
        }
        stats::CompensatedSum s;
        for (double x : v) s.add(x);
        ZTH_EXPECT_NEAR(s.value(), static_cast<double>(exact), 2e-3);
    }
}
ZTH_TEST("robust", "the correlation of huge values neither overflows nor leaves [-1, 1]") {
    stats::RunningCov c;
    for (int i = 0; i < 10; ++i) c.add(1e150 * i, 3e150 * i + 1e150);
    ZTH_EXPECT_NEAR(val(c.correlation()), 1.0, 1e-12);
    stats::RunningCov tiny;
    for (int i = 0; i < 10; ++i) tiny.add(1e-160 * i, -2e-160 * i);
    ZTH_EXPECT_NEAR(val(tiny.correlation()), -1.0, 1e-12);
    zth::Lcg rng(4);
    for (int round = 0; round < 500; ++round) {
        stats::RunningCov r;
        const double k = (rng.below(2) ? 1.0 : -1.0) * (0.1 + rng.below(100) / 7.0);
        for (int i = 0; i < 50; ++i) {
            const double x = rng.below(1000) / 3.0;
            r.add(x, k * x);   // perfectly linear: rounding may give 1.0000000000000002
        }
        const double rho = val(r.correlation());
        ZTH_EXPECT_TRUE(rho >= -1.0 && rho <= 1.0);
        ZTH_EXPECT_NEAR(rho, k > 0 ? 1.0 : -1.0, 1e-9);
    }
}
ZTH_TEST("robust", "percentile at the extremes: huge values, the last rank, and tiny fractions") {
    ZTH_EXPECT_NEAR(val(stats::percentile(std::vector<double>{-1e308, 1e308}, 50)), 0.0, 0);   // b - a overflows
    ZTH_EXPECT_NEAR(val(stats::percentile(std::vector<double>{-1e308, 1e308}, 100)), 1e308, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(std::vector<double>{3, 1, 2}, 100)), 3.0, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(std::vector<double>{3, 1, 2}, 99.999999)), 2.99999998, 1e-9);
    std::vector<double> v;
    for (int i = 100; i >= 1; --i) v.push_back(i);   // 100 .. 1, descending
    ZTH_EXPECT_NEAR(val(stats::percentile(v, 50)), 50.5, 1e-12);
    ZTH_EXPECT_NEAR(val(stats::percentile(v, 99)), 99.01, 1e-12);
    ZTH_EXPECT_NEAR(val(stats::percentile(v, 1)), 1.99, 1e-12);
    zth::Lcg rng(6);
    std::vector<double> w;
    for (int i = 0; i < 1001; ++i) w.push_back(rng.below(100000) / 10.0);
    std::vector<double> sorted = w;
    std::sort(sorted.begin(), sorted.end());
    for (int p = 0; p <= 100; ++p) {
        const double rank = p / 100.0 * 1000.0;
        const std::size_t lo = static_cast<std::size_t>(rank);
        const double f = rank - static_cast<double>(lo);
        const double want = lo + 1 < sorted.size() ? sorted[lo] * (1 - f) + sorted[lo + 1] * f : sorted[lo];
        ZTH_EXPECT_NEAR(val(stats::percentile(w, p)), want, 1e-9);
    }
}
ZTH_TEST("robust", "ties and duplicates are fine") {
    ZTH_EXPECT_NEAR(val(stats::median(std::vector<double>(7, 3.25))), 3.25, 0);
    ZTH_EXPECT_NEAR(val(stats::percentile(std::vector<double>{1, 1, 1, 5}, 75)), 2.0, 1e-12);   // rank 2.25 -> 1 + 0.25 * 4
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "add, merge and the readers allocate nothing") {
    stats::Running r, other;
    stats::RunningCov c;
    stats::CompensatedSum s;
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 100000; ++i) {
        const double x = i * 0.5;
        r.add(x);
        other.add(-x);
        c.add(x, x * x);
        s.add(x);
        sink += r.variance().value_or(0) + c.correlation().value_or(0);
    }
    r.merge(other);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sink > 0 && s.value() > 0 && r.count() == 200000);
}
ZTH_TEST("fast", "percentile allocates one copy of the data and never more") {
    std::vector<double> v(100000);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<double>((i * 2654435761u) % 100003);
    zth::mem::Scope scope;
    const auto p = stats::percentile(v, 37.5);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    ZTH_EXPECT_TRUE(p.has_value());
}
ZTH_TEST_TIMEOUT("fast", "a million values, then 40 percentiles of a million values", 30) {
    stats::Running r;
    stats::CompensatedSum s;
    for (int i = 0; i < 1000000; ++i) {
        r.add(i % 977);
        s.add(i % 977);
    }
    ZTH_EXPECT_EQ(r.count(), std::size_t{1000000});
    std::vector<double> v(1000000);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<double>((i * 2654435761u) % 1000003);
    double acc = 0;
    for (int p = 0; p < 40; ++p) acc += val(stats::percentile(v, p * 2.5));
    ZTH_EXPECT_TRUE(acc > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("stats.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("stats.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a selection, not a sort; no pow for squares; no sum-of-squares accumulators") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("stats.hpp"));
    for (const char* word : {"sort", "stable_sort", "partial_sort", "pow", "multiset", "set", "priority_queue", "accumulate", "reduce"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "nth_element").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "isfinite").empty());
}
