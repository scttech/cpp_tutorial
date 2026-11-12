#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>

#include "quat.hpp"
#include "zth_test.hpp"

using namespace gfx;

namespace {

constexpr double kPi = 3.14159265358979323846;
const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

bool near(Vec3 a, Vec3 b, double eps = 1e-12) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps; }
bool near(Quat a, Quat b, double eps = 1e-12) { return std::fabs(a.w - b.w) <= eps && std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps; }
bool same_rotation(Quat a, Quat b, double eps = 1e-12) { return near(a, b, eps) || near(a, -b, eps); }
Quat axis_angle(Vec3 axis, double a) { return *from_axis_angle(axis, a); }
Vec3 rv(zth::Lcg& rng) { return {(rng.below(2001) - 1000) / 100.0, (rng.below(2001) - 1000) / 100.0, (rng.below(2001) - 1000) / 100.0}; }
Quat random_rotation(zth::Lcg& rng) {
    Vec3 axis = rv(rng);
    if (!(length(axis) > 0.05)) axis = {1, 1, 1};
    return axis_angle(axis, (rng.below(6283) - 3141) / 1000.0);
}
Mat3 mat_mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k) r[static_cast<std::size_t>(i * 3 + j)] += a[static_cast<std::size_t>(i * 3 + k)] * b[static_cast<std::size_t>(k * 3 + j)];
    return r;
}
bool near(const Mat3& a, const Mat3& b, double eps = 1e-12) {
    for (std::size_t i = 0; i < 9; ++i)
        if (!(std::fabs(a[i] - b[i]) <= eps)) return false;
    return true;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "from_axis_angle uses the half angle; the axis need not be a unit vector; a zero axis has no rotation") {
    const Quat q = axis_angle({0, 0, 1}, kPi / 2);
    ZTH_EXPECT_TRUE(near(q, {std::sqrt(0.5), 0, 0, std::sqrt(0.5)}, 1e-15));
    ZTH_EXPECT_TRUE(near(axis_angle({0, 0, 5}, kPi / 2), q, 1e-15));
    ZTH_EXPECT_TRUE(near(axis_angle({1, 0, 0}, kPi), {0, 1, 0, 0}, 1e-15));
    ZTH_EXPECT_TRUE(near(axis_angle({0, 1, 0}, 0.0), {1, 0, 0, 0}, 0));
    ZTH_EXPECT_TRUE(near(axis_angle({0, 0, 1}, -kPi / 2), {std::sqrt(0.5), 0, 0, -std::sqrt(0.5)}, 1e-15));
    ZTH_EXPECT_FALSE(from_axis_angle({0, 0, 0}, 1.0).has_value());
    ZTH_EXPECT_FALSE(from_axis_angle({kNan, 0, 1}, 1.0).has_value());
    ZTH_EXPECT_FALSE(from_axis_angle({kInf, 0, 0}, 1.0).has_value());
}
ZTH_TEST("works", "rotating vectors: the right-hand rule about each axis, and the axis itself stays put") {
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({0, 0, 1}, kPi / 2), {1, 0, 0}), {0, 1, 0}));
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({1, 0, 0}, kPi / 2), {0, 1, 0}), {0, 0, 1}));
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({0, 1, 0}, kPi / 2), {0, 0, 1}), {1, 0, 0}));
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({0, 0, 1}, kPi), {1, 2, 3}), {-1, -2, 3}));
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({1, 1, 1}, 2 * kPi / 3), {1, 0, 0}), {0, 1, 0}));
    ZTH_EXPECT_TRUE(near(rotate(axis_angle({1, 2, 3}, 1.234), {1, 2, 3}), {1, 2, 3}));
    ZTH_EXPECT_TRUE(near(rotate(Quat{}, {4, 5, 6}), {4, 5, 6}, 0));
}
ZTH_TEST("works", "the Hamilton product composes rotations: (a * b) rotates by b first, and it does not commute") {
    const Quat rz = axis_angle({0, 0, 1}, kPi / 2), rx = axis_angle({1, 0, 0}, kPi / 2);
    const Vec3 v{0, 1, 0};
    ZTH_EXPECT_TRUE(near(rotate(rz * rx, v), rotate(rz, rotate(rx, v))));
    ZTH_EXPECT_TRUE(near(rotate(rx * rz, v), rotate(rx, rotate(rz, v))));
    ZTH_EXPECT_FALSE(near(rotate(rz * rx, v), rotate(rx * rz, v)));
    ZTH_EXPECT_TRUE(near(rz * Quat{}, rz, 0) && near(Quat{} * rz, rz, 0));
    ZTH_EXPECT_TRUE(near(Quat{0, 1, 0, 0} * Quat{0, 0, 1, 0}, {0, 0, 0, 1}, 0));   // i * j = k
    ZTH_EXPECT_TRUE(near(Quat{0, 0, 1, 0} * Quat{0, 1, 0, 0}, {0, 0, 0, -1}, 0));  // j * i = -k
    ZTH_EXPECT_TRUE(near(Quat{0, 1, 0, 0} * Quat{0, 1, 0, 0}, {-1, 0, 0, 0}, 0));  // i * i = -1
}
ZTH_TEST("works", "conjugate, norm, normalized and inverse") {
    const Quat q{1, 2, 3, 4};
    ZTH_EXPECT_TRUE(near(conjugate(q), {1, -2, -3, -4}, 0));
    ZTH_EXPECT_NEAR(norm(q), std::sqrt(30.0), 1e-15);
    ZTH_EXPECT_NEAR(norm(Quat{}), 1.0, 0);
    const auto n = normalized(q);
    ZTH_EXPECT_TRUE(n.has_value() && std::fabs(norm(*n) - 1.0) < 1e-15 && near(*n, {1 / std::sqrt(30.0), 2 / std::sqrt(30.0), 3 / std::sqrt(30.0), 4 / std::sqrt(30.0)}, 1e-15));
    ZTH_EXPECT_FALSE(normalized(Quat{0, 0, 0, 0}).has_value());
    ZTH_EXPECT_FALSE(normalized(Quat{kNan, 0, 0, 0}).has_value());
    ZTH_EXPECT_FALSE(normalized(Quat{1, kInf, 0, 0}).has_value());
    const auto inv = inverse(q);   // a non-unit quaternion
    ZTH_EXPECT_TRUE(inv.has_value() && near(q * *inv, {1, 0, 0, 0}, 1e-15) && near(*inv * q, {1, 0, 0, 0}, 1e-15));
    const Quat unit = axis_angle({1, 2, 2}, 0.9);
    ZTH_EXPECT_TRUE(near(*inverse(unit), conjugate(unit), 1e-15));
    ZTH_EXPECT_FALSE(inverse(Quat{0, 0, 0, 0}).has_value());
    ZTH_EXPECT_FALSE(inverse(Quat{1, kNan, 0, 0}).has_value());
}
ZTH_TEST("works", "q and -q are the same rotation (the double cover)") {
    const Quat q = axis_angle({1, 2, 3}, 2.5);
    const Vec3 v{1, -2, 0.5};
    ZTH_EXPECT_TRUE(near(rotate(q, v), rotate(-q, v)));
    ZTH_EXPECT_NEAR(angle_between(q, -q), 0.0, 1e-12);
    ZTH_EXPECT_TRUE(near(to_matrix(q), to_matrix(-q)));
}
ZTH_TEST("works", "to_matrix: the matrix of a 90 degree rotation about z, and agreement with rotate") {
    const Mat3 m = to_matrix(axis_angle({0, 0, 1}, kPi / 2));
    ZTH_EXPECT_TRUE(near(m, Mat3{0, -1, 0, 1, 0, 0, 0, 0, 1}, 1e-15));
    ZTH_EXPECT_TRUE(near(to_matrix(Quat{}), Mat3{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0));
    const Quat q = axis_angle({1, 2, 3}, 0.8);
    ZTH_EXPECT_TRUE(near(mul_vec(to_matrix(q), {1, 2, 3}), rotate(q, {1, 2, 3})));
    ZTH_EXPECT_TRUE(near(mul_vec(to_matrix(q), {-1, 0.5, 2}), rotate(q, {-1, 0.5, 2})));
    ZTH_EXPECT_TRUE(near(to_matrix(axis_angle({1, 0, 0}, kPi / 2)), Mat3{1, 0, 0, 0, 0, -1, 0, 1, 0}, 1e-15));
    ZTH_EXPECT_TRUE(near(to_matrix(axis_angle({0, 1, 0}, kPi / 2)), Mat3{0, 0, 1, 0, 1, 0, -1, 0, 0}, 1e-15));
}
ZTH_TEST("works", "from_matrix: known rotations come back with w >= 0, and non-rotations are refused") {
    const auto q = from_matrix(Mat3{0, -1, 0, 1, 0, 0, 0, 0, 1});
    ZTH_EXPECT_TRUE(q.has_value() && near(*q, {std::sqrt(0.5), 0, 0, std::sqrt(0.5)}, 1e-15));
    const auto id = from_matrix(Mat3{1, 0, 0, 0, 1, 0, 0, 0, 1});
    ZTH_EXPECT_TRUE(id.has_value() && near(*id, {1, 0, 0, 0}, 0));
    const auto neg = from_matrix(to_matrix(Quat{-std::sqrt(0.5), 0, 0, -std::sqrt(0.5)}));   // the same rotation as q, other sign
    ZTH_EXPECT_TRUE(neg.has_value() && neg->w >= 0 && near(*neg, *q, 1e-15));
    ZTH_EXPECT_FALSE(from_matrix(Mat3{-1, 0, 0, 0, 1, 0, 0, 0, 1}).has_value());   // a mirror image (det = -1)
    ZTH_EXPECT_FALSE(from_matrix(Mat3{2, 0, 0, 0, 2, 0, 0, 0, 2}).has_value());    // a scale
    ZTH_EXPECT_FALSE(from_matrix(Mat3{1, 1, 0, 0, 1, 0, 0, 0, 1}).has_value());    // a shear
    ZTH_EXPECT_FALSE(from_matrix(Mat3{kNan, 0, 0, 0, 1, 0, 0, 0, 1}).has_value());
    ZTH_EXPECT_FALSE(from_matrix(Mat3{0, 0, 0, 0, 0, 0, 0, 0, 0}).has_value());
}
ZTH_TEST("works", "from_matrix on half-turns: the trace is -1 and w is 0, so dividing by w would fail") {
    for (Vec3 axis : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}, Vec3{1, 1, 0}, Vec3{1, 0, -1}, Vec3{0, 1, 1}, Vec3{1, 2, 3}, Vec3{-1, -1, -1}, Vec3{1, 3, 2}, Vec3{3, 1, 2}, Vec3{-3, 1, -2}, Vec3{1, -3, 2}}) {
        const Quat q = axis_angle(axis, kPi);
        const auto back = from_matrix(to_matrix(q));
        ZTH_EXPECT_TRUE(back.has_value());
        if (!back) return;
        ZTH_EXPECT_TRUE(back->w >= 0);
        ZTH_EXPECT_NEAR(norm(*back), 1.0, 1e-12);
        ZTH_EXPECT_TRUE(same_rotation(*back, q, 1e-12));
    }
}
ZTH_TEST("works", "slerp: the endpoints, the middle of a quarter turn, and unit length") {
    const Quat a{}, b = axis_angle({0, 0, 1}, kPi / 2);
    ZTH_EXPECT_TRUE(near(slerp(a, b, 0.0), a, 1e-15));
    ZTH_EXPECT_TRUE(near(slerp(a, b, 1.0), b, 1e-15));
    ZTH_EXPECT_TRUE(near(slerp(a, b, 0.5), axis_angle({0, 0, 1}, kPi / 4), 1e-14));
    ZTH_EXPECT_TRUE(near(slerp(a, b, 0.25), axis_angle({0, 0, 1}, kPi / 8), 1e-14));
    for (double t : {0.0, 0.1, 0.5, 0.9, 1.0}) ZTH_EXPECT_NEAR(norm(slerp(a, b, t)), 1.0, 1e-14);
    const Quat c = axis_angle({1, 0, 0}, 1.0), d = axis_angle({0, 1, 0}, 2.0);   // two different axes
    ZTH_EXPECT_TRUE(same_rotation(slerp(c, d, 0.0), c, 1e-14) && same_rotation(slerp(c, d, 1.0), d, 1e-14));
}
ZTH_TEST("works", "slerp takes the short way round: q and -q give the same path") {
    const Quat a{}, b = axis_angle({0, 0, 1}, 0.8);
    const Quat mid = slerp(a, b, 0.5);
    ZTH_EXPECT_TRUE(near(mid, axis_angle({0, 0, 1}, 0.4), 1e-14));
    const Quat mid_neg = slerp(a, -b, 0.5);   // the same orientation written with the other sign
    ZTH_EXPECT_TRUE(same_rotation(mid_neg, mid, 1e-14));
    ZTH_EXPECT_NEAR(angle_between(a, mid_neg), 0.4, 1e-12);   // not 2 pi - 0.4 over two
    const Quat far = axis_angle({0, 0, 1}, 5.5);   // 5.5 rad is really -0.78 rad
    ZTH_EXPECT_NEAR(angle_between(a, slerp(a, far, 0.5)), (2 * kPi - 5.5) / 2, 1e-12);
}
ZTH_TEST("works", "angle_between: the rotation angle between two orientations, in 0 .. pi") {
    ZTH_EXPECT_NEAR(angle_between(Quat{}, axis_angle({0, 0, 1}, 1.0)), 1.0, 1e-14);
    ZTH_EXPECT_NEAR(angle_between(axis_angle({0, 0, 1}, 0.5), axis_angle({0, 0, 1}, 1.5)), 1.0, 1e-14);
    ZTH_EXPECT_NEAR(angle_between(axis_angle({1, 0, 0}, 0.3), axis_angle({1, 0, 0}, -0.4)), 0.7, 1e-14);
    ZTH_EXPECT_NEAR(angle_between(Quat{}, axis_angle({0, 1, 0}, kPi)), kPi, 1e-14);
    ZTH_EXPECT_NEAR(angle_between(Quat{}, axis_angle({0, 1, 0}, 3 * kPi / 2)), kPi / 2, 1e-14);   // 270 degrees is 90 the other way
    ZTH_EXPECT_NEAR(angle_between(Quat{}, Quat{}), 0.0, 0);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "rotate, to_matrix and composition agree on 500 random rotations") {
    zth::Lcg rng(21);
    for (int round = 0; round < 500; ++round) {
        const Quat a = random_rotation(rng), b = random_rotation(rng);
        const Vec3 v = rv(rng);
        ZTH_EXPECT_TRUE(near(mul_vec(to_matrix(a), v), rotate(a, v), 1e-11));
        ZTH_EXPECT_TRUE(near(to_matrix(a * b), mat_mul(to_matrix(a), to_matrix(b)), 1e-12));
        ZTH_EXPECT_TRUE(near(rotate(a * b, v), rotate(a, rotate(b, v)), 1e-11));
        ZTH_EXPECT_NEAR(length(rotate(a, v)), length(v), 1e-11);
        ZTH_EXPECT_TRUE(near(rotate(conjugate(a), rotate(a, v)), v, 1e-11));
        ZTH_EXPECT_NEAR(norm(a * b), 1.0, 1e-14);
    }
}
ZTH_TEST("robust", "from_matrix inverts to_matrix for random rotations, and for angles very close to 180 degrees") {
    zth::Lcg rng(8);
    for (int round = 0; round < 600; ++round) {
        Vec3 axis = rv(rng);
        if (!(length(axis) > 0.05)) axis = {3, -1, 2};
        double angle = (rng.below(6283) - 3141) / 1000.0;
        if (round % 3 == 0) angle = kPi - rng.below(100) * 1e-9;   // within 1e-7 of a half turn
        if (round % 7 == 0) angle = 0.0 + rng.below(100) * 1e-9;
        const Quat q = axis_angle(axis, angle);
        const auto back = from_matrix(to_matrix(q));
        ZTH_EXPECT_TRUE(back.has_value());
        if (!back) return;
        ZTH_EXPECT_TRUE(back->w >= 0.0);
        ZTH_EXPECT_NEAR(norm(*back), 1.0, 1e-12);
        ZTH_EXPECT_TRUE(same_rotation(*back, q, 1e-8));
        ZTH_EXPECT_TRUE(near(to_matrix(*back), to_matrix(q), 1e-12));
    }
}
ZTH_TEST("robust", "from_matrix tolerates matrices that are rotations up to rounding noise") {
    zth::Lcg rng(66);
    for (int round = 0; round < 200; ++round) {
        const Quat q = random_rotation(rng);
        Mat3 m = to_matrix(q);
        for (double& e : m) e += (rng.below(2001) - 1000) * 1e-12;
        const auto back = from_matrix(m);
        ZTH_EXPECT_TRUE(back.has_value() && same_rotation(*back, q, 1e-9));
    }
}
ZTH_TEST("robust", "slerp moves at constant angular speed (a plain lerp would not), and ends where it should") {
    zth::Lcg rng(5);
    for (int round = 0; round < 300; ++round) {
        const Quat a = random_rotation(rng), b = random_rotation(rng);
        const double total = angle_between(a, b);
        if (total < 0.2) continue;
        for (int i = 1; i <= 9; ++i) {
            const double t = i / 10.0;
            const Quat q = slerp(a, b, t);
            ZTH_EXPECT_NEAR(norm(q), 1.0, 1e-12);
            ZTH_EXPECT_NEAR(angle_between(a, q), t * total, 1e-9);
            ZTH_EXPECT_NEAR(angle_between(q, b), (1 - t) * total, 1e-9);
        }
        ZTH_EXPECT_TRUE(same_rotation(slerp(a, b, 1.0), b, 1e-12));
    }
}
ZTH_TEST("robust", "slerp is exact (constant speed) for moderate angles too: nlerp is only good for tiny ones") {
    zth::Lcg rng(12);
    for (double angle : {0.07, 0.1, 0.2, 0.3, 0.5, 0.8, 1.5, 3.0}) {
        for (int round = 0; round < 10; ++round) {
            Vec3 axis = rv(rng);
            if (!(length(axis) > 0.05)) axis = {0, 1, 0};
            const Quat a = random_rotation(rng);
            const Quat b = a * axis_angle(axis, angle);
            ZTH_EXPECT_NEAR(angle_between(a, b), angle, 1e-12);
            for (double t : {0.1, 0.3, 0.5, 0.7}) ZTH_EXPECT_NEAR(angle_between(a, slerp(a, b, t)), t * angle, 1e-9);
        }
    }
}
ZTH_TEST("robust", "slerp of nearly equal orientations is stable: no NaN, continuous across the switch to the linear formula") {
    const Quat a = axis_angle({1, 2, 3}, 0.7);
    for (double eps : {0.0, 1e-12, 1e-9, 1e-6, 1e-4, 0.02, 0.0316, 0.0317, 0.04, 0.1}) {
        const Quat b = axis_angle({1, 2, 3}, 0.7 + eps);
        for (double t : {0.0, 0.3, 0.5, 1.0}) {
            const Quat q = slerp(a, b, t);
            ZTH_EXPECT_TRUE(std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z));
            ZTH_EXPECT_NEAR(norm(q), 1.0, 1e-12);
            ZTH_EXPECT_TRUE(same_rotation(q, axis_angle({1, 2, 3}, 0.7 + t * eps), 1e-4 * eps + 1e-12));
        }
    }
    ZTH_EXPECT_TRUE(near(slerp(a, a, 0.5), a, 1e-14));
    const Quat almost = Quat{a.w * (1 + 4e-16), a.x, a.y, a.z};   // dot(a, almost) can exceed 1 by rounding
    ZTH_EXPECT_TRUE(std::isfinite(slerp(a, almost, 0.5).w));
}
ZTH_TEST("robust", "slerp between opposite orientations (dot = 0) and near-antipodal representations") {
    const Quat a{}, b = axis_angle({0, 0, 1}, kPi);   // dot(a, b) = 0: a half turn apart
    const Quat m = slerp(a, b, 0.5);
    ZTH_EXPECT_NEAR(angle_between(a, m), kPi / 2, 1e-12);
    ZTH_EXPECT_TRUE(near(m, axis_angle({0, 0, 1}, kPi / 2), 1e-12));
    const Quat c = axis_angle({0, 1, 0}, 0.3);
    ZTH_EXPECT_TRUE(same_rotation(slerp(c, -c, 0.5), c, 1e-12));   // the same orientation twice: stays put
    ZTH_EXPECT_NEAR(norm(slerp(c, -c, 0.37)), 1.0, 1e-12);
}
ZTH_TEST("robust", "angle_between is accurate for tiny angles and always within 0 .. pi") {
    for (double e : {1e-3, 1e-6, 1e-9, 1e-12}) ZTH_EXPECT_NEAR(angle_between(Quat{}, axis_angle({0, 0, 1}, e)) / e, 1.0, 1e-9);   // acos would give 0
    zth::Lcg rng(2);
    for (int i = 0; i < 1000; ++i) {
        const double t = angle_between(random_rotation(rng), random_rotation(rng));
        ZTH_EXPECT_TRUE(t >= 0.0 && t <= kPi + 1e-12);
    }
}
ZTH_TEST("robust", "norm, normalized and inverse of huge and tiny quaternions; drift is repaired by normalizing") {
    ZTH_EXPECT_NEAR(norm(Quat{1e200, 1e200, 0, 0}) / 1e200, std::sqrt(2.0), 1e-15);
    ZTH_EXPECT_NEAR(norm(Quat{1e-200, 1e-200, 1e-200, 1e-200}) / 1e-200, 2.0, 1e-15);
    const auto big = normalized(Quat{3e300, 0, 4e300, 0});
    ZTH_EXPECT_TRUE(big.has_value() && near(*big, {0.6, 0, 0.8, 0}, 1e-15));
    const auto tiny = normalized(Quat{0, 0, 0, 2e-310});
    ZTH_EXPECT_TRUE(tiny.has_value() && std::fabs(tiny->z - 1.0) < 1e-6);
    const auto inv_big = inverse(Quat{1e200, 0, 0, 0});
    ZTH_EXPECT_TRUE(inv_big.has_value() && std::fabs(inv_big->w * 1e200 - 1.0) < 1e-14);   // norm^2 would overflow
    const auto inv_tiny = inverse(Quat{0, 1e-200, 0, 0});
    ZTH_EXPECT_TRUE(inv_tiny.has_value() && std::fabs(inv_tiny->x * -1e-200 - 1.0) < 1e-14);   // norm^2 would underflow to 0
    Quat q = axis_angle({1, 2, 3}, 0.001);
    const Quat step = axis_angle({1, 2, 3}, 0.001);
    for (int i = 0; i < 200000; ++i) q = q * step;
    ZTH_EXPECT_TRUE(std::fabs(norm(q) - 1.0) < 1e-8);
    const auto fixed = normalized(q);
    ZTH_EXPECT_TRUE(fixed.has_value() && std::fabs(norm(*fixed) - 1.0) < 1e-15);
    ZTH_EXPECT_TRUE(near(rotate(*fixed, {1, 2, 3}), {1, 2, 3}, 1e-6));   // the axis is fixed
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "quaternion operations never allocate") {
    zth::Lcg rng(1);
    const Quat a = random_rotation(rng), b = random_rotation(rng);
    zth::mem::Scope scope;
    double sink = 0;
    Quat q = a;
    for (int i = 0; i < 5000; ++i) {
        q = slerp(a, b, (i % 100) / 100.0) * q;
        q = normalized(q).value_or(a);
        const Mat3 m = to_matrix(q);
        const auto back = from_matrix(m);
        sink += rotate(q, {1, 2, 3}).x + angle_between(a, b) + (back ? back->w : 0) + inverse(q).value_or(a).w;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(std::isfinite(sink));
}
ZTH_TEST_TIMEOUT("fast", "a million slerps, rotations and matrix round trips", 20) {
    zth::Lcg rng(4);
    const Quat a = random_rotation(rng), b = random_rotation(rng);
    double sink = 0;
    for (int i = 0; i < 1000000; ++i) {
        const Quat q = slerp(a, b, (i % 1000) / 1000.0);
        sink += rotate(q, {1.0, 0.5, 0.25}).z;
    }
    for (int i = 0; i < 200000; ++i) {
        const Quat q = slerp(a, b, (i % 1000) / 1000.0);
        const auto back = from_matrix(to_matrix(q));
        sink += back ? back->w : 0.0;
    }
    ZTH_EXPECT_TRUE(std::isfinite(sink));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no heap containers") {
    ZTH_EXPECT_NO_RAW_MEMORY("quat.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("quat.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "vector", "new", "function", "virtual", "pow"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "rotations are applied with the quaternion formula, not by building a matrix; tiny angles use atan2") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("quat.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "atan2").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "hypot").empty());
    // rotate() must not call to_matrix
    const std::size_t at = code.find("inline Vec3 rotate");
    ZTH_EXPECT_TRUE(at != std::string::npos);
    const std::string body = code.substr(at, code.find("inline Mat3 to_matrix") - at);
    ZTH_EXPECT_TRUE(::zth::find_word(body, "to_matrix").empty() && ::zth::find_word(body, "mul_vec").empty());
}
