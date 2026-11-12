#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "numerics.hpp"
#include "zth_test.hpp"

using numeric::Interp;
using numeric::Root;
using numeric::Spline;
using Vec = std::vector<double>;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();
double val(const std::optional<double>& o) { return o.value_or(kNan); }
double rx(const std::optional<Root>& r) { return r ? r->x : kNan; }

// brute-force linear interpolation: scan the segments from the left
double ref_linear(const Vec& xs, const Vec& ys, double x) {
    for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        if (x >= xs[i] && x <= xs[i + 1]) {
            const double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
            return ys[i] * (1 - t) + ys[i + 1] * t;
        }
    return kNan;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "linear interpolation: knots, mid-points, slopes of both signs, the ends") {
    const auto t = Interp::make({0, 1, 2, 4}, {0, 2, 1, 5});
    ZTH_EXPECT_TRUE(t.has_value());
    ZTH_EXPECT_EQ(t->size(), std::size_t{4});
    ZTH_EXPECT_NEAR(val((*t)(0.0)), 0.0, 0);
    ZTH_EXPECT_NEAR(val((*t)(1.0)), 2.0, 0);   // exactly the table's value at a knot
    ZTH_EXPECT_NEAR(val((*t)(2.0)), 1.0, 0);
    ZTH_EXPECT_NEAR(val((*t)(4.0)), 5.0, 0);   // the last knot belongs to the last segment
    ZTH_EXPECT_NEAR(val((*t)(0.5)), 1.0, 1e-15);
    ZTH_EXPECT_NEAR(val((*t)(1.5)), 1.5, 1e-15);   // a falling segment
    ZTH_EXPECT_NEAR(val((*t)(3.0)), 3.0, 1e-15);   // a wider segment
    ZTH_EXPECT_NEAR(val((*t)(3.999)), 4.998, 1e-12);
}
ZTH_TEST("works", "linear interpolation: outside the table and NaN have no value") {
    const auto t = Interp::make({-1, 0, 3}, {1, 2, 3});
    ZTH_EXPECT_TRUE(t.has_value());
    ZTH_EXPECT_FALSE((*t)(-1.0001).has_value());
    ZTH_EXPECT_FALSE((*t)(3.0001).has_value());
    ZTH_EXPECT_FALSE((*t)(kNan).has_value());
    ZTH_EXPECT_FALSE((*t)(kInf).has_value());
    ZTH_EXPECT_FALSE((*t)(-kInf).has_value());
    ZTH_EXPECT_TRUE((*t)(-1.0).has_value() && (*t)(3.0).has_value());
}
ZTH_TEST("works", "tables that cannot be interpolated are refused") {
    ZTH_EXPECT_FALSE(Interp::make({}, {}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1}, {1}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2}, {1}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2}, {1, 2, 3}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2, 3}, {1, 2}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 1}, {1, 2}).has_value());          // not strictly increasing
    ZTH_EXPECT_FALSE(Interp::make({1, 3, 2}, {1, 2, 3}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2, 3}, {1, kNan, 3}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, kNan, 3}, {1, 2, 3}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2, kInf}, {1, 2, 3}).has_value());
    ZTH_EXPECT_FALSE(Interp::make({1, 2, 3}, {1, 2, kInf}).has_value());
    ZTH_EXPECT_TRUE(Interp::make({1, 2}, {5, 5}).has_value());
    ZTH_EXPECT_FALSE(Spline::make({}, {}).has_value());
    ZTH_EXPECT_FALSE(Spline::make({1}, {1}).has_value());
    ZTH_EXPECT_FALSE(Spline::make({1, 2, 2}, {1, 2, 3}).has_value());
    ZTH_EXPECT_FALSE(Spline::make({1, 2, 3}, {1, kNan, 3}).has_value());
}
ZTH_TEST("works", "the natural cubic spline: values and derivatives against exact rational arithmetic") {
    const auto s = Spline::make({0, 1, 2, 3, 4.5}, {0, 1, 0, 2, 1});
    ZTH_EXPECT_TRUE(s.has_value());
    ZTH_EXPECT_NEAR(val((*s)(0.0)), 0.0, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(0.5)), 0.7940140845070423, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(1.0)), 1.0, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(1.5)), 0.36795774647887325, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(2.5)), 0.8591549295774648, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(3.7)), 2.1864037558685445, 1e-13);
    ZTH_EXPECT_NEAR(val((*s)(4.5)), 1.0, 1e-13);
    ZTH_EXPECT_NEAR(val(s->derivative(0.0)), 1.784037558685446, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(0.5)), 1.1960093896713615, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(1.0)), -0.568075117370892, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(1.5)), -1.4800469483568075, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(2.5)), 2.4741784037558685, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(3.7)), -0.833990610328639, 1e-12);
    ZTH_EXPECT_NEAR(val(s->derivative(4.5)), -1.807511737089202, 1e-12);
    ZTH_EXPECT_FALSE((*s)(4.6).has_value());
    ZTH_EXPECT_FALSE(s->derivative(-0.1).has_value());
    ZTH_EXPECT_FALSE((*s)(kNan).has_value());
}
ZTH_TEST("works", "a spline through two points is a line; through three points it is the natural spline") {
    const auto line = Spline::make({1, 3}, {2, 8});
    ZTH_EXPECT_TRUE(line.has_value());
    ZTH_EXPECT_NEAR(val((*line)(2.0)), 5.0, 1e-13);
    ZTH_EXPECT_NEAR(val(line->derivative(1.7)), 3.0, 1e-13);
    const auto s3 = Spline::make({0, 1, 3}, {0, 2, 1});
    ZTH_EXPECT_TRUE(s3.has_value());
    ZTH_EXPECT_NEAR(val((*s3)(0.5)), 1.15625, 1e-13);
    ZTH_EXPECT_NEAR(val((*s3)(2.0)), 2.125, 1e-13);
    ZTH_EXPECT_NEAR(val(s3->derivative(0.5)), 2.1041666666666665, 1e-12);
    ZTH_EXPECT_NEAR(val(s3->derivative(2.0)), -0.7083333333333334, 1e-12);
}
ZTH_TEST("works", "bisection finds roots of polynomials and transcendental functions, in either order of the bracket") {
    auto sq = [](double x) { return x * x - 2.0; };
    const auto r = numeric::bisect(sq, 0.0, 2.0);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->x, 1.4142135623730951, 1e-11);
    ZTH_EXPECT_TRUE(r->iterations >= 35 && r->iterations <= 45);
    ZTH_EXPECT_NEAR(rx(numeric::bisect(sq, 2.0, 0.0)), 1.4142135623730951, 1e-11);   // swapped
    ZTH_EXPECT_NEAR(rx(numeric::bisect(sq, -3.0, 0.0)), -1.4142135623730951, 1e-11);
    ZTH_EXPECT_NEAR(rx(numeric::bisect([](double x) { return std::cos(x) - x; }, 0.0, 1.0)), 0.7390851332151607, 1e-11);
    ZTH_EXPECT_NEAR(rx(numeric::bisect([](double x) { return 1.0 - x * x * x; }, 0.0, 5.0)), 1.0, 1e-11);   // decreasing: the sign pattern is reversed
}
ZTH_TEST("works", "bisection: a root at an end is returned at once; no sign change, bad input and poles are refused") {
    auto id = [](double x) { return x; };
    const auto at_a = numeric::bisect(id, 0.0, 5.0);
    ZTH_EXPECT_TRUE(at_a.has_value() && at_a->x == 0.0 && at_a->iterations == 0);
    const auto at_b = numeric::bisect(id, -5.0, 0.0);
    ZTH_EXPECT_TRUE(at_b.has_value() && at_b->x == 0.0 && at_b->iterations == 0);
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return x * x + 1.0; }, -1.0, 1.0).has_value());   // no sign change
    ZTH_EXPECT_FALSE(numeric::bisect(id, kNan, 1.0).has_value());
    ZTH_EXPECT_FALSE(numeric::bisect(id, -1.0, kInf).has_value());
    ZTH_EXPECT_FALSE(numeric::bisect(id, -1.0, 1.0, -1e-9).has_value());
    ZTH_EXPECT_FALSE(numeric::bisect(id, -1.0, 1.0, kNan).has_value());
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return 1.0 / x; }, -1.0, 2.0).has_value());   // a pole, not a root
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return std::tan(x); }, 1.0, 2.0).has_value());
    ZTH_EXPECT_NEAR(rx(numeric::bisect([](double x) { return std::tan(x); }, 3.0, 4.0)), 3.141592653589793, 1e-10);   // a genuine root of tan
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return x == 0.0 ? kNan : -1.0; }, 0.0, 2.0).has_value());   // NaN at one end
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return x == 2.0 ? kInf : -1.0; }, 0.0, 2.0).has_value());   // infinity at the other
    ZTH_EXPECT_FALSE(numeric::bisect([](double x) { return std::fabs(x - 0.5) < 0.3 ? kNan : x - 0.5; }, 0.0, 2.0).has_value());   // NaN at the first midpoint's neighbourhood
}
ZTH_TEST("works", "bisection: max_iter and the tolerance are honoured") {
    auto sq = [](double x) { return x * x - 2.0; };
    ZTH_EXPECT_FALSE(numeric::bisect(sq, 0.0, 2.0, 1e-12, 10).has_value());   // 10 halvings are not enough
    const auto coarse = numeric::bisect(sq, 0.0, 2.0, 1e-3);
    ZTH_EXPECT_TRUE(coarse.has_value() && coarse->iterations <= 11);
    ZTH_EXPECT_NEAR(coarse->x, 1.4142135623730951, 1e-3);
    int calls = 0;
    const auto counted = numeric::bisect([&calls](double x) { ++calls; return x * x - 2.0; }, 0.0, 2.0, 1e-9);
    ZTH_EXPECT_TRUE(counted.has_value());
    ZTH_EXPECT_TRUE(calls <= counted->iterations + 3);   // two ends, one per iteration, one to check the answer
}
ZTH_TEST("works", "Newton: fast convergence from a good start, an exact root at the start, and the usual failures") {
    auto f = [](double x) { return x * x - 2.0; };
    auto df = [](double x) { return 2.0 * x; };
    const auto r = numeric::newton(f, df, 1.0);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->x, 1.4142135623730951, 1e-14);
    ZTH_EXPECT_TRUE(r->iterations >= 4 && r->iterations <= 7);
    const auto exact = numeric::newton(f, df, 0.0 + 1.4142135623730951);
    ZTH_EXPECT_TRUE(exact.has_value() && std::fabs(exact->x - 1.4142135623730951) < 1e-15);
    const auto at_root = numeric::newton([](double x) { return x - 3.0; }, [](double) { return 1.0; }, 3.0);
    ZTH_EXPECT_TRUE(at_root.has_value() && at_root->x == 3.0 && at_root->iterations == 0);
    ZTH_EXPECT_FALSE(numeric::newton([](double x) { return x * x - 1.0; }, [](double x) { return 2.0 * x; }, 0.0).has_value());   // f'(x0) = 0
    ZTH_EXPECT_FALSE(numeric::newton([](double x) { return std::atan(x); }, [](double x) { return 1.0 / (1.0 + x * x); }, 1.5).has_value());   // diverges
    ZTH_EXPECT_FALSE(numeric::newton([](double x) { return x * x + 1.0; }, [](double x) { return 2.0 * x; }, 3.0).has_value());   // no real root: wanders until max_iter
    ZTH_EXPECT_FALSE(numeric::newton(f, [](double) { return kNan; }, 1.0).has_value());
    ZTH_EXPECT_FALSE(numeric::newton([](double) { return kInf; }, df, 1.0).has_value());
}
ZTH_TEST("works", "safeguarded Newton converges where plain Newton diverges, and needs few steps where Newton works") {
    auto g = [](double x) { return std::atan(x); };
    auto dg = [](double x) { return 1.0 / (1.0 + x * x); };
    const auto r = numeric::safe_newton(g, dg, -10.0, 20.0);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->x, 0.0, 1e-9);
    const auto cube = numeric::safe_newton([](double x) { return std::cbrt(x); }, [](double x) { return 1.0 / (3.0 * std::cbrt(x * x)); }, -1.0, 8.0);   // plain Newton doubles x each step
    ZTH_EXPECT_TRUE(cube.has_value());
    ZTH_EXPECT_NEAR(cube->x, 0.0, 1e-9);
    const auto fast = numeric::safe_newton([](double x) { return x * x - 2.0; }, [](double x) { return 2.0 * x; }, 0.0, 2.0);
    ZTH_EXPECT_TRUE(fast.has_value());
    ZTH_EXPECT_NEAR(fast->x, 1.4142135623730951, 1e-12);
    ZTH_EXPECT_TRUE(fast->iterations <= 12);   // bisection needs 41
    const auto poly = numeric::safe_newton([](double x) { return x * x * x - 2.0 * x - 5.0; }, [](double x) { return 3.0 * x * x - 2.0; }, 2.0, 3.0);
    ZTH_EXPECT_NEAR(rx(poly), 2.0945514815423265, 1e-12);
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton([](double x) { return std::cos(x) - x; }, [](double x) { return -std::sin(x) - 1.0; }, 1.0, 0.0)), 0.7390851332151607, 1e-12);   // swapped
}
ZTH_TEST("robust", "Newton's stopping rule is relative for large roots: a root near 1e12 is found") {
    const auto r = numeric::newton([](double x) { return x * x - 2e24; }, [](double x) { return 2.0 * x; }, 3e12);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->x / 1e12, 1.4142135623730951, 1e-9);
}
ZTH_TEST("robust", "safeguarded Newton falls back to bisection when Newton crawls: x^21 within 100 iterations (plain Newton contracts by only 20/21 per step)") {
    const auto r = numeric::safe_newton([](double x) { return std::pow(x, 21.0); }, [](double x) { return 21.0 * std::pow(x, 20.0); }, -1.0, 2.0, 1e-9, 100);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(rx(r), 0.0, 1e-7);
}
ZTH_TEST("works", "safeguarded Newton: the same refusals as bisection") {
    auto f = [](double x) { return x * x + 1.0; };
    auto df = [](double x) { return 2.0 * x; };
    ZTH_EXPECT_FALSE(numeric::safe_newton(f, df, -1.0, 1.0).has_value());
    ZTH_EXPECT_FALSE(numeric::safe_newton([](double x) { return x - 1.0; }, [](double) { return 1.0; }, kNan, 3.0).has_value());
    const auto end = numeric::safe_newton([](double x) { return x - 1.0; }, [](double) { return 1.0; }, 1.0, 3.0);
    ZTH_EXPECT_TRUE(end.has_value() && end->x == 1.0 && end->iterations == 0);
    // a derivative that is wrong (zero everywhere) must not stop the search: it falls back to bisection
    const auto no_help = numeric::safe_newton([](double x) { return x * x - 2.0; }, [](double) { return 0.0; }, 0.0, 2.0);
    ZTH_EXPECT_TRUE(no_help.has_value());
    ZTH_EXPECT_NEAR(no_help->x, 1.4142135623730951, 1e-11);
    const auto nan_d = numeric::safe_newton([](double x) { return x * x - 2.0; }, [](double) { return kNan; }, 0.0, 2.0);
    ZTH_EXPECT_TRUE(nan_d.has_value());
    ZTH_EXPECT_NEAR(nan_d->x, 1.4142135623730951, 1e-11);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "linear interpolation against a brute-force scan on random tables, at knots, between them and at the extremes") {
    zth::Lcg rng(321);
    for (int round = 0; round < 200; ++round) {
        const int n = 2 + rng.below(30);
        Vec xs, ys;
        double x = rng.below(21) - 10.0;
        for (int i = 0; i < n; ++i) {
            xs.push_back(x);
            ys.push_back(rng.below(2001) / 10.0 - 100.0);
            x += 0.001 + rng.below(1000) / 100.0;
        }
        const auto t = Interp::make(xs, ys);
        ZTH_EXPECT_TRUE(t.has_value());
        for (int q = 0; q < 60; ++q) {
            const double qx = xs.front() + (xs.back() - xs.front()) * (rng.below(10001) / 10000.0);
            ZTH_EXPECT_NEAR(val((*t)(qx)), ref_linear(xs, ys, qx), 1e-12);
        }
        for (int i = 0; i < n; ++i) ZTH_EXPECT_NEAR(val((*t)(xs[static_cast<std::size_t>(i)])), ys[static_cast<std::size_t>(i)], 0);
    }
}
ZTH_TEST("robust", "huge and tiny values: interpolation does not overflow and tiny segments still work") {
    const auto big = Interp::make({0, 1}, {-1.7e308, 1.7e308});
    ZTH_EXPECT_TRUE(big.has_value());
    ZTH_EXPECT_NEAR(val((*big)(0.5)), 0.0, 0);   // y0 + t * (y1 - y0) would be infinite
    ZTH_EXPECT_TRUE(std::isfinite(val((*big)(0.25))));
    const auto tiny = Interp::make({1.0, 1.0 + 1e-15, 1.0 + 2e-15}, {0, 10, 0});
    ZTH_EXPECT_TRUE(tiny.has_value());
    ZTH_EXPECT_NEAR(val((*tiny)(1.0 + 1e-15)), 10.0, 0);
    const auto wide = Interp::make({-1e300, 0, 1e300}, {1, 2, 3});
    ZTH_EXPECT_TRUE(wide.has_value());
    ZTH_EXPECT_NEAR(val((*wide)(-5e299)), 1.5, 1e-12);
    ZTH_EXPECT_NEAR(val((*wide)(1e300)), 3.0, 0);
}
ZTH_TEST("robust", "the spline reproduces its table, reproduces straight lines, and is C1 (and C2) across the knots") {
    zth::Lcg rng(17);
    for (int round = 0; round < 100; ++round) {
        const int n = 3 + rng.below(12);
        Vec xs, ys, line;
        double x = 0;
        for (int i = 0; i < n; ++i) {
            xs.push_back(x);
            ys.push_back(rng.below(2001) / 100.0 - 10.0);
            line.push_back(3.0 * x - 7.0);
            x += 0.2 + rng.below(100) / 25.0;
        }
        const auto s = Spline::make(xs, ys);
        const auto l = Spline::make(xs, line);
        ZTH_EXPECT_TRUE(s.has_value() && l.has_value());
        for (int i = 0; i < n; ++i) ZTH_EXPECT_NEAR(val((*s)(xs[static_cast<std::size_t>(i)])), ys[static_cast<std::size_t>(i)], 1e-9);
        for (int q = 0; q < 20; ++q) {
            const double qx = xs.back() * (rng.below(10001) / 10000.0);
            ZTH_EXPECT_NEAR(val((*l)(qx)), 3.0 * qx - 7.0, 1e-9);
            ZTH_EXPECT_NEAR(val(l->derivative(qx)), 3.0, 1e-9);
        }
        for (int i = 1; i + 1 < n; ++i) {   // continuity of value and slope just left and right of an interior knot
            const double k = xs[static_cast<std::size_t>(i)], e = 1e-9;
            ZTH_EXPECT_NEAR(val((*s)(k - e)), val((*s)(k + e)), 1e-5);
            ZTH_EXPECT_NEAR(val(s->derivative(k - e)), val(s->derivative(k + e)), 1e-4);
        }
        // natural boundary: the second derivative vanishes, so the slope is stationary near the ends
        const double e = 1e-4;
        const double d0 = val(s->derivative(xs.front())), d1 = val(s->derivative(xs.front() + e));
        ZTH_EXPECT_TRUE(std::fabs(d1 - d0) < 5e-1);
    }
}
ZTH_TEST("robust", "the spline approximates a smooth function much better than the polyline does") {
    Vec xs, ys;
    for (int i = 0; i <= 16; ++i) {
        xs.push_back(i * 0.4);
        ys.push_back(std::sin(i * 0.4));
    }
    const auto spline = Spline::make(xs, ys);
    const auto poly = Interp::make(xs, ys);
    ZTH_EXPECT_TRUE(spline.has_value() && poly.has_value());
    double worst_spline = 0, worst_poly = 0;
    for (int q = 8; q <= 6 * 100 - 8; ++q) {   // away from the ends, where the natural boundary condition is a poor fit for sin
        const double x = q / 100.0 + 0.0;
        worst_spline = std::max(worst_spline, std::fabs(val((*spline)(x)) - std::sin(x)));
        worst_poly = std::max(worst_poly, std::fabs(val((*poly)(x)) - std::sin(x)));
    }
    ZTH_EXPECT_TRUE(worst_spline < 4e-3);
    ZTH_EXPECT_TRUE(worst_poly > 5 * worst_spline);
}
ZTH_TEST("robust", "bisection: 300 random monotone functions, brackets in both orders, increasing and decreasing") {
    zth::Lcg rng(44);
    for (int round = 0; round < 300; ++round) {
        const double root = rng.below(20001) / 100.0 - 100.0;
        const double slope = (rng.below(2) ? 1.0 : -1.0) * (0.01 + rng.below(1000) / 10.0);
        auto f = [=](double x) { return slope * (x - root) * (1.0 + 0.001 * (x - root) * (x - root)); };
        double a = root - (0.01 + rng.below(5000) / 10.0), b = root + (0.01 + rng.below(5000) / 10.0);
        if (rng.below(2)) std::swap(a, b);
        const auto r = numeric::bisect(f, a, b, 1e-10);
        ZTH_EXPECT_TRUE(r.has_value());
        ZTH_EXPECT_NEAR(rx(r), root, 1e-9);
    }
}
ZTH_TEST("robust", "bisection with tol = 0 terminates at the neighbouring doubles; with huge brackets it does not overflow") {
    auto sq = [](double x) { return x * x - 2.0; };
    const auto r = numeric::bisect(sq, 0.0, 2.0, 0.0);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->x, 1.4142135623730951, 4e-16);
    ZTH_EXPECT_TRUE(r->iterations <= 80);
    const auto huge = numeric::bisect([](double x) { return x - 1e300; }, -1.7e308, 1.7e308, 1e290, 400);   // a + b overflows; (b - a) too
    ZTH_EXPECT_TRUE(huge.has_value());
    ZTH_EXPECT_NEAR(huge->x / 1e300, 1.0, 1e-3);
    const auto high = numeric::bisect([](double x) { return x - 1.5e308; }, 1e308, 1.7e308, 1e290);   // a + b is infinite
    ZTH_EXPECT_TRUE(high.has_value());
    ZTH_EXPECT_NEAR(high->x / 1e308, 1.5, 1e-3);
    const auto zero_mid = numeric::bisect([](double x) { return x; }, -1.7e308, 1.7e308);   // the midpoint is exactly the root
    ZTH_EXPECT_TRUE(zero_mid.has_value() && zero_mid->x == 0.0);
}
ZTH_TEST("robust", "safeguarded Newton on awkward functions: flat regions, a double root, steep tails, an inflection") {
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton([](double x) { return std::tanh(x - 3.0); }, [](double x) { const double t = std::tanh(x - 3.0); return 1 - t * t; }, -50.0, 60.0)), 3.0, 1e-9);   // flat tails: Newton from the ends goes nowhere
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton([](double x) { return std::exp(x) - 1e6; }, [](double x) { return std::exp(x); }, 0.0, 100.0)), std::log(1e6), 1e-9);
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton([](double x) { return x * x * x; }, [](double x) { return 3 * x * x; }, -1.0, 2.0, 1e-9)), 0.0, 1e-2);   // a triple root: slow but safe, within the tolerance
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton([](double x) { return (x - 1) * (x - 1) * (x - 1) + 0.001; }, [](double x) { return 3 * (x - 1) * (x - 1); }, 0.0, 1.0)), 1.0 - std::cbrt(0.001), 1e-8);
    zth::Lcg rng(6);
    for (int round = 0; round < 200; ++round) {
        const double c = rng.below(2000) / 100.0 - 10.0;
        const auto r = numeric::safe_newton([=](double x) { return std::atan(x - c); }, [=](double x) { return 1.0 / (1.0 + (x - c) * (x - c)); }, c - 30.0, c + 25.0);
        ZTH_EXPECT_NEAR(rx(r), c, 1e-9);
    }
}
ZTH_TEST("robust", "Newton on a function with a known basin: converges inside, refuses outside, never loops forever") {
    auto f = [](double x) { return x * x * x - 2.0 * x + 2.0; };   // the famous cycle 0 <-> 1 for plain Newton
    auto df = [](double x) { return 3.0 * x * x - 2.0; };
    ZTH_EXPECT_FALSE(numeric::newton(f, df, 0.0).has_value());     // 0 -> 1 -> 0 -> 1 ...
    ZTH_EXPECT_NEAR(rx(numeric::newton(f, df, -2.0)), -1.7692923542386314, 1e-12);
    ZTH_EXPECT_NEAR(rx(numeric::safe_newton(f, df, -3.0, 3.0)), -1.7692923542386314, 1e-12);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "evaluating a table, a spline or a root finder allocates nothing") {
    Vec xs, ys;
    for (int i = 0; i < 100; ++i) {
        xs.push_back(i);
        ys.push_back(i * 0.5);
    }
    const auto t = Interp::make(xs, ys);
    const auto s = Spline::make(xs, ys);
    ZTH_EXPECT_TRUE(t.has_value() && s.has_value());
    auto f = [](double x) { return x * x - 2.0; };
    auto df = [](double x) { return 2.0 * x; };
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 1000; ++i) {
        sink += val((*t)(i * 0.0987)) + val((*s)(i * 0.0987)) + val(s->derivative(i * 0.0987));
        sink += rx(numeric::bisect(f, 0.0, 2.0)) + rx(numeric::newton(f, df, 1.0)) + rx(numeric::safe_newton(f, df, 0.0, 2.0));
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sink > 0);
}
ZTH_TEST("fast", "building a spline allocates a handful of arrays, not one per knot") {
    Vec xs, ys;
    for (int i = 0; i < 5000; ++i) {
        xs.push_back(i * 0.5);
        ys.push_back(std::sin(i * 0.5));
    }
    zth::mem::Scope scope;
    const auto s = Spline::make(std::move(xs), std::move(ys));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 6);
    ZTH_EXPECT_TRUE(s.has_value());
}
ZTH_TEST("fast", "root finders call f few times: bisection log2(range / tol), Newton a handful, safe Newton close to Newton") {
    int calls = 0;
    auto f = [&calls](double x) { ++calls; return std::cos(x) - x; };
    int dcalls = 0;
    auto df = [&dcalls](double x) { ++dcalls; return -std::sin(x) - 1.0; };
    const auto b = numeric::bisect(f, 0.0, 1.0, 1e-12);
    ZTH_EXPECT_TRUE(b.has_value() && calls <= 45);
    calls = 0;
    const auto n = numeric::newton(f, df, 0.5);
    ZTH_EXPECT_TRUE(n.has_value() && calls <= 8 && dcalls <= 8);
    calls = dcalls = 0;
    const auto s = numeric::safe_newton(f, df, 0.0, 1.0);
    ZTH_EXPECT_TRUE(s.has_value() && calls <= 14 && dcalls <= 14);
}
ZTH_TEST_TIMEOUT("fast", "a million-knot table queried a million times: a binary search, not a scan", 20) {
    const std::size_t n = 1000000;
    Vec xs(n), ys(n);
    for (std::size_t i = 0; i < n; ++i) {
        xs[i] = static_cast<double>(i);
        ys[i] = static_cast<double>(i % 17);
    }
    const auto t = Interp::make(std::move(xs), std::move(ys));
    ZTH_EXPECT_TRUE(t.has_value());
    zth::Lcg rng(2);
    double sink = 0;
    for (int i = 0; i < 1000000; ++i) sink += val((*t)(static_cast<double>(rng.next() % 999999) + 0.5));
    ZTH_EXPECT_TRUE(sink > 0);
    const auto s = Spline::make([] { Vec v(100000); for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<double>(i); return v; }(),
                                [] { Vec v(100000); for (std::size_t i = 0; i < v.size(); ++i) v[i] = std::sin(static_cast<double>(i) * 0.01); return v; }());
    ZTH_EXPECT_TRUE(s.has_value());
    for (int i = 0; i < 300000; ++i) sink += val((*s)((rng.next() % 99999) + 0.25));
    ZTH_EXPECT_TRUE(std::isfinite(sink));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("numerics.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("numerics.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "callables are template parameters (no std::function); segments are found by binary search, not a scan") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("numerics.hpp"));
    for (const char* word : {"function", "find", "find_if", "count", "accumulate", "pow"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "upper_bound").empty() && ::zth::find_word(code, "lower_bound").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "template").empty());
}
