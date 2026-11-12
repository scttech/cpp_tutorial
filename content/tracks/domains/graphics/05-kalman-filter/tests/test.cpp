#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <string>

#include "kalman.hpp"
#include "zth_test.hpp"

using namespace kf;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

template <std::size_t N>
bool symmetric_exactly(const Mat<N>& m) {
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            if (m(i, j) != m(j, i)) return false;
    return true;
}
template <std::size_t N>
bool same(const Mat<N>& a, const Mat<N>& b, double eps) {
    for (std::size_t i = 0; i < N * N; ++i)
        if (!(std::fabs(a.a[i] - b.a[i]) <= eps)) return false;
    return true;
}
template <std::size_t N>
bool same(const Vec<N>& a, const Vec<N>& b, double eps) {
    for (std::size_t i = 0; i < N; ++i)
        if (!(std::fabs(a[i] - b[i]) <= eps)) return false;
    return true;
}
// positive semi-definite for 1x1, 2x2 and 3x3, by the leading principal minors (with a tolerance relative to the diagonal)
bool psd2(const Mat<2>& p) {
    const double tol = 1e-12 * (std::fabs(p(0, 0)) + std::fabs(p(1, 1))) * (std::fabs(p(0, 0)) + std::fabs(p(1, 1)));
    return p(0, 0) >= 0 && p(1, 1) >= 0 && p(0, 0) * p(1, 1) - p(0, 1) * p(1, 0) >= -tol;
}

Mat<2> m2(double a, double b, double c, double d) { return Mat<2>{{a, b, c, d}}; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "the constant-velocity model: F and the white-noise-acceleration Q") {
    const Model m = constant_velocity(0.5, 2.0);
    ZTH_EXPECT_TRUE(same(m.F, m2(1, 0.5, 0, 1), 0));
    ZTH_EXPECT_TRUE(same(m.Q, m2(2.0 * 0.0625 / 4, 2.0 * 0.125 / 2, 2.0 * 0.125 / 2, 2.0 * 0.25), 1e-15));
    const Model unit = constant_velocity(1.0, 1.0);
    ZTH_EXPECT_TRUE(same(unit.Q, m2(0.25, 0.5, 0.5, 1.0), 1e-15));
    ZTH_EXPECT_TRUE(same(constant_velocity(0.0, 3.0).F, m2(1, 0, 0, 1), 0) && same(constant_velocity(0.0, 3.0).Q, m2(0, 0, 0, 0), 0));
}
ZTH_TEST("works", "a scalar random-walk filter, by hand: innovation, gain, state and covariance") {
    auto k = Kalman<1>::make({0.0}, Mat<1>{{1.0}});
    ZTH_EXPECT_TRUE(k.has_value());
    const auto r = k->update({1.0}, 2.0, 1.0);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_NEAR(r->y, 2.0, 1e-15);
    ZTH_EXPECT_NEAR(r->s, 2.0, 1e-15);   // P + R
    ZTH_EXPECT_NEAR(r->nis, 2.0, 1e-15);  // y^2 / s = 4 / 2
    ZTH_EXPECT_NEAR(k->state()[0], 1.0, 1e-15);   // K = 0.5
    ZTH_EXPECT_NEAR(k->covariance()(0, 0), 0.5, 1e-15);
    ZTH_EXPECT_TRUE(k->predict(Mat<1>{{1.0}}, Mat<1>{{0.25}}));
    ZTH_EXPECT_NEAR(k->state()[0], 1.0, 1e-15);
    ZTH_EXPECT_NEAR(k->covariance()(0, 0), 0.75, 1e-15);   // the uncertainty grows by Q
    ZTH_EXPECT_TRUE(k->predict(Mat<1>{{2.0}}, Mat<1>{{0.0}}));   // x' = 2 x, P' = 4 P
    ZTH_EXPECT_NEAR(k->state()[0], 2.0, 1e-15);
    ZTH_EXPECT_NEAR(k->covariance()(0, 0), 3.0, 1e-15);
}
ZTH_TEST("works", "a two-state update with a position measurement: the velocity is corrected through the covariance") {
    auto k = Kalman<2>::make({0.0, 0.0}, m2(1, 0.5, 0.5, 1));
    ZTH_EXPECT_TRUE(k.has_value());
    const auto r = k->update({1.0, 0.0}, 1.0, 1.0);   // S = 1 + 1 = 2, K = (0.5, 0.25)
    ZTH_EXPECT_TRUE(r.has_value() && std::fabs(r->s - 2.0) < 1e-15);
    ZTH_EXPECT_TRUE(same(k->state(), Vec<2>{0.5, 0.25}, 1e-15));
    // P' = P - K S K^T = [[1 - 0.5, 0.5 - 0.25], [0.5 - 0.25, 1 - 0.125]]
    ZTH_EXPECT_TRUE(same(k->covariance(), m2(0.5, 0.25, 0.25, 0.875), 1e-15));
}
ZTH_TEST("works", "tracking with golden numbers from an independent reference implementation") {
    auto f = Kalman<2>::make({0.0, 1.0}, m2(10, 0, 0, 10));
    ZTH_EXPECT_TRUE(f.has_value());
    const Model m = constant_velocity(0.1, 0.5);
    for (int i = 1; i <= 50; ++i) {
        ZTH_EXPECT_TRUE(f->predict(m.F, m.Q));
        ZTH_EXPECT_TRUE(f->update({1.0, 0.0}, 0.1 * i + 0.3 * std::sin(i), 0.25).has_value());
        if (i == 1) {
            ZTH_EXPECT_TRUE(same(f->state(), Vec<2>{0.3463436869746891, 1.024396531479187}, 1e-12));
            ZTH_EXPECT_TRUE(same(f->covariance(), m2(0.2439613599500484, 0.024160598839856474, 0.024160598839856474, 9.908333444041736), 1e-12));
        }
        if (i == 5) {
            ZTH_EXPECT_TRUE(same(f->state(), Vec<2>{0.2603251123349256, -0.24199355113738258}, 1e-11));
            ZTH_EXPECT_TRUE(same(f->covariance(), m2(0.1295732784117422, 0.39644322433137114, 0.39644322433137114, 1.973426501179174), 1e-11));
        }
    }
    ZTH_EXPECT_TRUE(same(f->state(), Vec<2>{4.952277949875387, 0.9611576338445005}, 1e-10));
    ZTH_EXPECT_TRUE(same(f->covariance(), m2(0.03871571897771493, 0.03254821503105303, 0.03254821503105303, 0.05710201767940217), 1e-12));
}
ZTH_TEST("works", "the covariance converges to the steady state (the Riccati fixed point), independent of the readings") {
    auto f = Kalman<2>::make({0.0, 0.0}, m2(1, 0, 0, 1));
    const Model m = constant_velocity(0.1, 0.5);
    for (int i = 0; i < 2000; ++i) {
        f->predict(m.F, m.Q);
        f->update({1.0, 0.0}, 0.0, 0.25);
    }
    ZTH_EXPECT_TRUE(same(f->covariance(), m2(0.03868896539126414, 0.03250469463083265, 0.03250469463083265, 0.05701288856989502), 1e-11));
    ZTH_EXPECT_TRUE(same(f->state(), Vec<2>{0.0, 0.0}, 1e-12));
}
ZTH_TEST("works", "the filter converges on a moving target and reports shrinking uncertainty") {
    auto f = Kalman<2>::make({0.0, 0.0}, m2(100, 0, 0, 100));
    const Model m = constant_velocity(0.1, 0.01);
    double var0 = f->covariance()(0, 0);
    for (int i = 1; i <= 200; ++i) {
        f->predict(m.F, m.Q);
        f->update({1.0, 0.0}, 0.5 * 0.1 * i, 0.0001);   // a noiseless ramp of velocity 0.5
    }
    ZTH_EXPECT_NEAR(f->state()[1], 0.5, 1e-2);
    ZTH_EXPECT_NEAR(f->state()[0], 0.5 * 0.1 * 200, 1e-2);
    ZTH_EXPECT_TRUE(f->covariance()(0, 0) < var0 / 1000);
}
ZTH_TEST("works", "outliers are gated out: an inlier at exactly the gate is accepted, an outlier changes nothing") {
    auto f = Kalman<1>::make({0.0}, Mat<1>{{1.0}});
    // S = 1 + 1 = 2; a reading z has nis = z^2 / 2
    const Vec<1> before_x = f->state();
    const Mat<1> before_p = f->covariance();
    ZTH_EXPECT_FALSE(f->update({1.0}, 5.0, 1.0, 9.0).has_value());   // nis 12.5
    ZTH_EXPECT_TRUE(same(f->state(), before_x, 0) && same(f->covariance(), before_p, 0));
    auto g = Kalman<1>::make({0.0}, Mat<1>{{1.0}});
    ZTH_EXPECT_TRUE(g->update({1.0}, 2.0, 1.0, 2.0).has_value());     // nis = 4 / 2 = 2 == gate: accepted
    auto h = Kalman<1>::make({0.0}, Mat<1>{{1.0}});
    ZTH_EXPECT_FALSE(h->update({1.0}, 2.0, 1.0, 1.999).has_value());
    ZTH_EXPECT_TRUE(h->update({1.0}, 2.0, 1.0).has_value());          // no gate by default
    auto z = Kalman<1>::make({0.0}, Mat<1>{{1.0}});
    ZTH_EXPECT_TRUE(z->update({1.0}, 0.0, 1.0, 0.0).has_value());     // nis = 0 == gate 0
    ZTH_EXPECT_FALSE(z->update({1.0}, 1e-3, 1.0, 0.0).has_value());
}
ZTH_TEST("works", "bad inputs are refused and change nothing: NaN readings, R <= 0, bad gates, non-finite models") {
    auto f = Kalman<2>::make({1.0, 2.0}, m2(1, 0, 0, 1));
    const Vec<2> x0 = f->state();
    const Mat<2> p0 = f->covariance();
    const Vec<2> H = {1.0, 0.0};
    ZTH_EXPECT_FALSE(f->update(H, kNan, 1.0).has_value());
    ZTH_EXPECT_FALSE(f->update(H, kInf, 1.0).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, 0.0).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, -1.0).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, kNan).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, kInf).has_value());
    ZTH_EXPECT_FALSE(f->update({kNan, 0.0}, 1.0, 1.0).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, 1.0, kNan).has_value());
    ZTH_EXPECT_FALSE(f->update(H, 1.0, 1.0, -1.0).has_value());
    ZTH_EXPECT_FALSE(f->predict(m2(1, kNan, 0, 1), m2(0, 0, 0, 0)));
    ZTH_EXPECT_FALSE(f->predict(m2(1, 0.1, 0, 1), m2(kInf, 0, 0, 0)));
    ZTH_EXPECT_TRUE(same(f->state(), x0, 0) && same(f->covariance(), p0, 0));
}
ZTH_TEST("works", "make() validates the initial state and covariance") {
    ZTH_EXPECT_FALSE(Kalman<2>::make({kNan, 0}, m2(1, 0, 0, 1)).has_value());
    ZTH_EXPECT_FALSE(Kalman<2>::make({0, 0}, m2(kInf, 0, 0, 1)).has_value());
    ZTH_EXPECT_FALSE(Kalman<2>::make({0, 0}, m2(-1, 0, 0, 1)).has_value());
    ZTH_EXPECT_FALSE(Kalman<2>::make({0, 0}, m2(1, 0.5, 0.4, 1)).has_value());   // not symmetric
    ZTH_EXPECT_TRUE(Kalman<2>::make({0, 0}, m2(1, 0.5, 0.5, 1)).has_value());
    ZTH_EXPECT_TRUE(Kalman<2>::make({0, 0}, m2(0, 0, 0, 0)).has_value());        // a perfectly known state
    ZTH_EXPECT_TRUE(Kalman<2>::make({0, 0}, m2(1e12, 1.0, 1.0 + 1e-4, 1e12)).has_value());   // symmetric up to 1e-4, which is nothing next to 1e12
    const auto k = Kalman<2>::make({0, 0}, m2(1e12, 1.0, 1.0 + 1e-4, 1e12));
    ZTH_EXPECT_TRUE(symmetric_exactly(k->covariance()));
}
ZTH_TEST("works", "the stored covariance is exactly symmetric after make, predict and update") {
    auto f = Kalman<3>::make({0, 0, 0}, Mat<3>{{4, 1, 0.5, 1, 3, 0.2, 0.5, 0.2, 2}});
    ZTH_EXPECT_TRUE(f.has_value() && symmetric_exactly(f->covariance()));
    Mat<3> F = identity<3>();
    F(0, 1) = 0.1;
    F(1, 2) = 0.1;
    F(0, 2) = 0.005;
    Mat<3> Q = identity<3>() * 0.01;
    for (int i = 0; i < 50; ++i) {
        ZTH_EXPECT_TRUE(f->predict(F, Q));
        ZTH_EXPECT_TRUE(symmetric_exactly(f->covariance()));
        ZTH_EXPECT_TRUE(f->update({1.0, 0.3, -0.2}, 0.1 * i, 0.5).has_value());
        ZTH_EXPECT_TRUE(symmetric_exactly(f->covariance()));
    }
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "the filter does not freeze: with a tiny R and a huge prior the covariance stays positive and later readings still move the state") {
    auto f = Kalman<2>::make({0, 0}, m2(1e4, 0, 0, 1e4));
    const Model m = constant_velocity(1.0, 0.0);
    for (int i = 0; i < 200; ++i) {
        f->predict(m.F, m.Q);
        f->update({1.0, 1.0}, 0.0, 1e-14);
    }
    const Mat<2> p = f->covariance();
    ZTH_EXPECT_TRUE(p(0, 0) > 0.0 && p(1, 1) > 0.0);   // the short form P = (I - KH) P collapses to exactly 0 here
    ZTH_EXPECT_TRUE(psd2(p));
    ZTH_EXPECT_TRUE(symmetric_exactly(p));
    f->predict(m.F, m.Q);
    const auto r = f->update({1.0, 1.0}, 5.0, 1e-14);
    ZTH_EXPECT_TRUE(r.has_value());
    const double moved = f->state()[0] + f->state()[1];
    ZTH_EXPECT_TRUE(moved > 0.01);   // a filter with P == 0 has gain 0 and ignores the new reading
}
ZTH_TEST("robust", "ten thousand steps with awkward numbers: P stays symmetric, positive semi-definite and finite") {
    zth::Lcg rng(77);
    auto f = Kalman<2>::make({0, 0}, m2(100, 0, 0, 100));
    for (int i = 0; i < 10000; ++i) {
        const double dt = 0.001 + rng.below(1000) / 1000.0;
        const Model m = constant_velocity(dt, rng.below(2) ? 1e-9 : 3.0);
        ZTH_EXPECT_TRUE(f->predict(m.F, m.Q));
        const double R = rng.below(3) == 0 ? 1e-10 : rng.below(2) ? 0.5 : 1e4;
        f->update({1.0, 0.0}, std::sin(i * 0.01) + (rng.below(1000) - 500) * 1e-3, R);
        const Mat<2> p = f->covariance();
        if (!(symmetric_exactly(p) && psd2(p) && is_finite(p) && is_finite(f->state()))) {
            zth::fail("covariance lost symmetry, positive semi-definiteness or finiteness at step " + std::to_string(i));
            return;
        }
    }
}
ZTH_TEST("robust", "a bad reading in the middle of the stream leaves the filter exactly as if it had not arrived") {
    const Model m = constant_velocity(0.1, 0.5);
    auto clean = Kalman<2>::make({0, 1}, m2(10, 0, 0, 10));
    auto dirty = Kalman<2>::make({0, 1}, m2(10, 0, 0, 10));
    for (int i = 1; i <= 100; ++i) {
        const double z = 0.1 * i + 0.2 * std::cos(i);
        clean->predict(m.F, m.Q);
        dirty->predict(m.F, m.Q);
        clean->update({1.0, 0.0}, z, 0.2);
        if (i % 7 == 0) dirty->update({1.0, 0.0}, kNan, 0.2);
        if (i % 11 == 0) dirty->update({1.0, 0.0}, z, -0.2);
        if (i % 13 == 0) dirty->update({1.0, 0.0}, 1e9, 0.2, 9.0);   // an outlier behind a gate
        dirty->update({1.0, 0.0}, z, 0.2);
    }
    ZTH_EXPECT_TRUE(same(clean->state(), dirty->state(), 0) && same(clean->covariance(), dirty->covariance(), 0));
}
ZTH_TEST("robust", "the gate makes the filter robust to outliers: a stream with 5% wild readings tracks as well as a clean one") {
    zth::Lcg rng(5);
    const Model m = constant_velocity(0.1, 0.1);
    auto gated = Kalman<2>::make({0, 0}, m2(10, 0, 0, 10));
    auto plain = Kalman<2>::make({0, 0}, m2(10, 0, 0, 10));
    double err_gated = 0, err_plain = 0;
    int rejected = 0;
    for (int i = 1; i <= 400; ++i) {
        const double truth = 0.3 * 0.1 * i;
        const double noise = (rng.below(2001) - 1000) * 1e-4;
        const double z = truth + noise + (rng.below(20) == 0 ? 50.0 : 0.0);
        gated->predict(m.F, m.Q);
        plain->predict(m.F, m.Q);
        if (!gated->update({1.0, 0.0}, z, 0.01, 16.0) && i > 20) ++rejected;
        plain->update({1.0, 0.0}, z, 0.01);
        if (i > 50) {
            err_gated += std::fabs(gated->state()[0] - truth);
            err_plain += std::fabs(plain->state()[0] - truth);
        }
    }
    ZTH_EXPECT_TRUE(rejected > 5 && rejected < 60);
    ZTH_EXPECT_TRUE(err_gated / 350 < 0.1);
    ZTH_EXPECT_TRUE(err_plain > 5 * err_gated);
}
ZTH_TEST("robust", "variable time steps: the same readings fed with different dt give different, consistent estimates; dt = 0 is a pure update") {
    auto a = Kalman<2>::make({0, 0}, m2(1, 0, 0, 1));
    auto b = Kalman<2>::make({0, 0}, m2(1, 0, 0, 1));
    for (int i = 1; i <= 20; ++i) {
        a->predict(constant_velocity(0.1, 1.0).F, constant_velocity(0.1, 1.0).Q);
        a->update({1.0, 0.0}, 0.1 * i, 0.05);
        b->predict(constant_velocity(0.2, 1.0).F, constant_velocity(0.2, 1.0).Q);
        b->update({1.0, 0.0}, 0.1 * i, 0.05);
    }
    ZTH_EXPECT_NEAR(a->state()[1], 1.0, 0.1);   // 0.1 units per 0.1 s
    ZTH_EXPECT_NEAR(b->state()[1], 0.5, 0.1);   // 0.1 units per 0.2 s
    auto z = Kalman<2>::make({0, 0}, m2(1, 0, 0, 1));
    const Mat<2> before = z->covariance();
    ZTH_EXPECT_TRUE(z->predict(constant_velocity(0.0, 5.0).F, constant_velocity(0.0, 5.0).Q));
    ZTH_EXPECT_TRUE(same(z->covariance(), before, 0));
}
ZTH_TEST("robust", "a three-state filter (position, velocity, acceleration) tracks a quadratic path") {
    auto f = Kalman<3>::make({0, 0, 0}, Mat<3>{{100, 0, 0, 0, 100, 0, 0, 0, 100}});
    const double dt = 0.1;
    Mat<3> F = identity<3>();
    F(0, 1) = dt;
    F(0, 2) = 0.5 * dt * dt;
    F(1, 2) = dt;
    const Mat<3> Q = identity<3>() * 1e-6;
    for (int i = 1; i <= 300; ++i) {
        f->predict(F, Q);
        const double t = dt * i;
        f->update({1.0, 0.0, 0.0}, 0.5 * 2.0 * t * t, 1e-4);   // position = t^2: velocity 2 t, acceleration 2
    }
    ZTH_EXPECT_NEAR(f->state()[0], 900.0, 0.1);
    ZTH_EXPECT_NEAR(f->state()[1], 60.0, 0.5);
    ZTH_EXPECT_NEAR(f->state()[2], 2.0, 0.1);
    ZTH_EXPECT_TRUE(symmetric_exactly(f->covariance()));
}
ZTH_TEST("robust", "measuring a combination of states, and two sensors fused one after the other") {
    auto f = Kalman<2>::make({0, 0}, m2(4, 0, 0, 4));
    ZTH_EXPECT_TRUE(f->update({1.0, 1.0}, 3.0, 1.0).has_value());   // we only learn the SUM of the two states
    ZTH_EXPECT_NEAR(f->state()[0], f->state()[1], 1e-15);          // symmetric prior: the correction is shared equally
    ZTH_EXPECT_NEAR(f->state()[0] + f->state()[1], 3.0 * 8.0 / 9.0, 1e-14);   // S = 8 + 1 = 9; the sum moves by 8/9 of the innovation
    ZTH_EXPECT_TRUE(f->update({1.0, -1.0}, 0.0, 1.0).has_value());  // and now their difference
    auto order_a = Kalman<2>::make({0, 0}, m2(4, 0, 0, 4));
    auto order_b = Kalman<2>::make({0, 0}, m2(4, 0, 0, 4));
    order_a->update({1.0, 0.0}, 1.0, 0.5);
    order_a->update({0.0, 1.0}, 2.0, 0.5);
    order_b->update({0.0, 1.0}, 2.0, 0.5);
    order_b->update({1.0, 0.0}, 1.0, 0.5);
    ZTH_EXPECT_TRUE(same(order_a->state(), order_b->state(), 1e-14));   // independent sensors: the order does not matter
    ZTH_EXPECT_TRUE(same(order_a->covariance(), order_b->covariance(), 1e-14));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "predict and update never allocate") {
    auto f = Kalman<4>::make({0, 0, 0, 0}, identity<4>());
    Mat<4> F = identity<4>();
    F(0, 1) = 0.1;
    F(2, 3) = 0.1;
    const Mat<4> Q = identity<4>() * 0.01;
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 20000; ++i) {
        f->predict(F, Q);
        const auto r = f->update({1.0, 0.0, 0.0, 0.0}, std::sin(i * 0.01), 0.1, 25.0);
        sink += r ? r->nis : 0.0;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(std::isfinite(sink));
}
ZTH_TEST_TIMEOUT("fast", "a million filter steps with a four-state model", 20) {
    auto f = Kalman<4>::make({0, 0, 0, 0}, identity<4>());
    Mat<4> F = identity<4>();
    F(0, 1) = 0.1;
    F(2, 3) = 0.1;
    const Mat<4> Q = identity<4>() * 0.01;
    for (int i = 0; i < 1000000; ++i) {
        f->predict(F, Q);
        f->update({1.0, 0.0, 0.0, 0.0}, std::sin(i * 0.001), 0.1);
        f->update({0.0, 0.0, 1.0, 0.0}, std::cos(i * 0.001), 0.1);
    }
    ZTH_EXPECT_TRUE(is_finite(f->covariance()) && is_finite(f->state()));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no heap containers") {
    ZTH_EXPECT_NO_RAW_MEMORY("kalman.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("kalman.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "vector", "new", "function", "virtual"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "no matrix inverse is needed for a scalar measurement; the Joseph form is spelled out (transposed(A) appears)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("kalman.hpp"));
    for (const char* word : {"inverse", "invert", "solve", "pow", "det", "determinant"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "transposed").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "outer").empty());
}
