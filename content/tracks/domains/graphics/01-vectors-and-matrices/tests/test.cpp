#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>

#include "mathlib.hpp"
#include "zth_test.hpp"

using namespace gfx;

namespace {

constexpr double kPi = 3.14159265358979323846;
const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

bool near(Vec3 a, Vec3 b, double eps = 1e-12) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps; }
bool near(const Mat4& a, const Mat4& b, double eps = 1e-9) {
    for (std::size_t i = 0; i < 16; ++i)
        if (!(std::fabs(a.m[i] - b.m[i]) <= eps)) return false;
    return true;
}
Vec3 pt(const Mat4& m, Vec3 p) {
    const auto r = m.transform_point(p);
    return r ? *r : Vec3{kNan, kNan, kNan};
}
double val(const std::optional<double>& o) { return o.value_or(kNan); }

Vec3 rv(zth::Lcg& rng, double range = 10.0) { return {(rng.below(2001) - 1000) * range / 1000.0, (rng.below(2001) - 1000) * range / 1000.0, (rng.below(2001) - 1000) * range / 1000.0}; }
Mat4 random_transform(zth::Lcg& rng) {
    Vec3 axis = rv(rng);
    if (!(length(axis) > 0.1)) axis = {0, 1, 0};
    const Mat4 r = *Mat4::rotation(axis, (rng.below(6283) - 3141) / 1000.0);
    const Vec3 s = {0.5 + rng.below(300) / 100.0, 0.5 + rng.below(300) / 100.0, 0.5 + rng.below(300) / 100.0};
    return Mat4::translation(rv(rng)) * r * Mat4::scaling(s);
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "vector arithmetic, dot and cross products") {
    const Vec3 a{1, 2, 3}, b{-2, 0.5, 4};
    ZTH_EXPECT_TRUE(near(a + b, {-1, 2.5, 7}, 0));
    ZTH_EXPECT_TRUE(near(a - b, {3, 1.5, -1}, 0));
    ZTH_EXPECT_TRUE(near(-a, {-1, -2, -3}, 0));
    ZTH_EXPECT_TRUE(near(a * 2.0, {2, 4, 6}, 0) && near(2.0 * a, {2, 4, 6}, 0));
    ZTH_EXPECT_TRUE(near(a / 2.0, {0.5, 1, 1.5}, 0));
    ZTH_EXPECT_TRUE(a == Vec3({1, 2, 3}) && !(a == b));
    ZTH_EXPECT_FALSE(Vec3({1, 2, 3}) == Vec3({1, 2, 4}));
    ZTH_EXPECT_FALSE(Vec3({1, 2, 3}) == Vec3({1, 5, 3}));
    ZTH_EXPECT_NEAR(dot(a, b), -2 + 1 + 12, 0);
    ZTH_EXPECT_TRUE(near(cross(a, b), {8 - 1.5, -6 - 4, 0.5 + 4}, 1e-15));
    ZTH_EXPECT_TRUE(near(cross({1, 0, 0}, {0, 1, 0}), {0, 0, 1}, 0));   // right-handed
    ZTH_EXPECT_TRUE(near(cross({0, 1, 0}, {1, 0, 0}), {0, 0, -1}, 0));
    ZTH_EXPECT_NEAR(dot(cross(a, b), a), 0.0, 1e-14);
}
ZTH_TEST("works", "length, normalized, lerp, reflect") {
    ZTH_EXPECT_NEAR(length({3, 4, 0}), 5.0, 1e-15);
    ZTH_EXPECT_NEAR(length({0, 0, 0}), 0.0, 0);
    ZTH_EXPECT_NEAR(length({-1, -2, -2}), 3.0, 1e-15);
    const auto n = normalized({0, 3, 4});
    ZTH_EXPECT_TRUE(n.has_value() && near(*n, {0, 0.6, 0.8}, 1e-15));
    ZTH_EXPECT_FALSE(normalized({0, 0, 0}).has_value());
    ZTH_EXPECT_TRUE(near(lerp({0, 0, 0}, {10, 20, 30}, 0.25), {2.5, 5, 7.5}, 1e-15));
    ZTH_EXPECT_TRUE(near(lerp({1, 1, 1}, {5, 5, 5}, 0), {1, 1, 1}, 0) && near(lerp({1, 1, 1}, {5, 5, 5}, 1), {5, 5, 5}, 0));
    ZTH_EXPECT_TRUE(near(reflect({1, -1, 0}, {0, 1, 0}), {1, 1, 0}, 1e-15));   // a ray bouncing off the floor
    ZTH_EXPECT_TRUE(near(reflect({0, 0, -1}, {0, 0, 1}), {0, 0, 1}, 0));
}
ZTH_TEST("works", "angle_between: right angle, parallel, opposite, and no angle for a zero vector") {
    ZTH_EXPECT_NEAR(val(angle_between({1, 0, 0}, {0, 1, 0})), kPi / 2, 1e-15);
    ZTH_EXPECT_NEAR(val(angle_between({1, 0, 0}, {5, 0, 0})), 0.0, 0);
    ZTH_EXPECT_NEAR(val(angle_between({1, 0, 0}, {-3, 0, 0})), kPi, 1e-15);
    ZTH_EXPECT_NEAR(val(angle_between({1, 1, 0}, {1, 0, 0})), kPi / 4, 1e-15);
    ZTH_EXPECT_NEAR(val(angle_between({0, 0, 2}, {0, 3, 3})), kPi / 4, 1e-15);
    ZTH_EXPECT_FALSE(angle_between({0, 0, 0}, {1, 0, 0}).has_value());
    ZTH_EXPECT_FALSE(angle_between({1, 0, 0}, {0, 0, 0}).has_value());
}
ZTH_TEST("works", "matrices are COLUMN-major: a translation lives in elements 12, 13, 14") {
    const Mat4 t = Mat4::translation({7, 8, 9});
    ZTH_EXPECT_TRUE(t.m[12] == 7 && t.m[13] == 8 && t.m[14] == 9 && t.m[15] == 1);
    ZTH_EXPECT_TRUE(t.m[0] == 1 && t.m[5] == 1 && t.m[10] == 1);
    ZTH_EXPECT_TRUE(t.at(0, 3) == 7 && t.at(1, 3) == 8 && t.at(2, 3) == 9);
    const Mat4 s = Mat4::scaling({2, 3, 4});
    ZTH_EXPECT_TRUE(s.m[0] == 2 && s.m[5] == 3 && s.m[10] == 4 && s.m[15] == 1 && s.m[12] == 0 && s.m[1] == 0);
    const Mat4 id = Mat4::identity();
    for (std::size_t i = 0; i < 16; ++i) ZTH_EXPECT_TRUE(id.m[i] == ((i % 5 == 0) ? 1.0 : 0.0));
    Mat4 m;
    m.at(1, 2) = 5;   // row 1, column 2
    ZTH_EXPECT_TRUE(m.m[2 * 4 + 1] == 5);
}
ZTH_TEST("works", "multiplication order: (A * B) applies B first; points are translated, directions are not") {
    const Mat4 t = Mat4::translation({10, 0, 0}), s = Mat4::scaling({2, 2, 2});
    ZTH_EXPECT_TRUE(near(pt(t * s, {1, 1, 1}), {12, 2, 2}));   // scale first, then move
    ZTH_EXPECT_TRUE(near(pt(s * t, {1, 1, 1}), {22, 2, 2}));   // move first, then scale
    ZTH_EXPECT_TRUE(near((t * s).transform_direction({1, 1, 1}), {2, 2, 2}));
    ZTH_EXPECT_TRUE(near(t.transform_direction({1, 2, 3}), {1, 2, 3}));
    ZTH_EXPECT_TRUE(near(Mat4::identity() * t, t, 0) && near(t * Mat4::identity(), t, 0));
}
ZTH_TEST("works", "rotations: 90 degrees about z, an unnormalised axis, the right-hand rule, and no rotation about a zero axis") {
    const auto rz = Mat4::rotation({0, 0, 1}, kPi / 2);
    ZTH_EXPECT_TRUE(rz.has_value());
    ZTH_EXPECT_TRUE(near(pt(*rz, {1, 0, 0}), {0, 1, 0}));
    ZTH_EXPECT_TRUE(near(pt(*rz, {0, 1, 0}), {-1, 0, 0}));
    const auto rx = Mat4::rotation({1, 0, 0}, kPi / 2);
    ZTH_EXPECT_TRUE(rx.has_value() && near(pt(*rx, {0, 1, 0}), {0, 0, 1}));
    const auto ry = Mat4::rotation({0, 1, 0}, kPi / 2);
    ZTH_EXPECT_TRUE(ry.has_value() && near(pt(*ry, {0, 0, 1}), {1, 0, 0}));
    const auto big_axis = Mat4::rotation({0, 0, 50}, kPi / 2);
    ZTH_EXPECT_TRUE(big_axis.has_value() && near(*big_axis, *rz));
    const auto diag = Mat4::rotation({1, 1, 1}, 2 * kPi / 3);   // cycles the axes: x -> y -> z -> x
    ZTH_EXPECT_TRUE(diag.has_value() && near(pt(*diag, {1, 0, 0}), {0, 1, 0}) && near(pt(*diag, {0, 1, 0}), {0, 0, 1}));
    ZTH_EXPECT_FALSE(Mat4::rotation({0, 0, 0}, 1.0).has_value());
    ZTH_EXPECT_FALSE(Mat4::rotation({kNan, 0, 1}, 1.0).has_value());
    ZTH_EXPECT_TRUE(near(*Mat4::rotation({0, 0, 1}, 0.0), Mat4::identity()));
}
ZTH_TEST("works", "transposed, determinant and inverse of simple matrices") {
    Mat4 m;
    for (std::size_t i = 0; i < 16; ++i) m.m[i] = static_cast<double>(i + 1);
    ZTH_EXPECT_TRUE(m.transposed().at(0, 1) == m.at(1, 0) && m.transposed().at(3, 2) == m.at(2, 3));
    ZTH_EXPECT_TRUE(near(m.transposed().transposed(), m, 0));
    ZTH_EXPECT_NEAR(Mat4::identity().determinant(), 1.0, 0);
    ZTH_EXPECT_NEAR(Mat4::scaling({2, 3, 4}).determinant(), 24.0, 1e-13);
    ZTH_EXPECT_NEAR(Mat4::translation({5, 6, 7}).determinant(), 1.0, 1e-13);
    ZTH_EXPECT_NEAR(Mat4::scaling({-1, 1, 1}).determinant(), -1.0, 0);   // a mirror
    const auto inv_t = Mat4::translation({1, 2, 3}).inverse();
    ZTH_EXPECT_TRUE(inv_t.has_value() && near(*inv_t, Mat4::translation({-1, -2, -3}), 1e-14));
    const auto inv_s = Mat4::scaling({2, 4, 0.5}).inverse();
    ZTH_EXPECT_TRUE(inv_s.has_value() && near(*inv_s, Mat4::scaling({0.5, 0.25, 2}), 1e-14));
    const Mat4 r = *Mat4::rotation({1, 2, 3}, 0.7);
    const auto inv_r = r.inverse();
    ZTH_EXPECT_TRUE(inv_r.has_value() && near(*inv_r, r.transposed(), 1e-13));
    ZTH_EXPECT_NEAR(r.determinant(), 1.0, 1e-13);
    ZTH_EXPECT_FALSE(m.inverse().has_value());   // 1..16 is singular
    ZTH_EXPECT_FALSE(Mat4::scaling({1, 0, 1}).inverse().has_value());
    ZTH_EXPECT_FALSE(Mat4().inverse().has_value());
}
ZTH_TEST("works", "a general (non-affine, non-orthogonal) matrix is inverted correctly") {
    Mat4 m;   // an integer matrix with determinant 27
    const double rows[4][4] = {{2, 1, 0, 1}, {0, 3, 1, 0}, {1, 0, 2, 1}, {0, 1, 0, 2}};
    for (std::size_t r = 0; r < 4; ++r)
        for (std::size_t c = 0; c < 4; ++c) m.at(r, c) = rows[r][c];
    ZTH_EXPECT_NEAR(m.determinant(), 27.0, 1e-12);
    const auto inv = m.inverse();
    ZTH_EXPECT_TRUE(inv.has_value());
    ZTH_EXPECT_TRUE(near(m * *inv, Mat4::identity(), 1e-12));
    ZTH_EXPECT_TRUE(near(*inv * m, Mat4::identity(), 1e-12));
}
ZTH_TEST("works", "look_at maps the eye to the origin and the target onto the -z axis") {
    const auto v = Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    ZTH_EXPECT_TRUE(v.has_value());
    ZTH_EXPECT_TRUE(near(pt(*v, {0, 0, 5}), {0, 0, 0}));
    ZTH_EXPECT_TRUE(near(pt(*v, {0, 0, 0}), {0, 0, -5}));
    ZTH_EXPECT_TRUE(near(pt(*v, {1, 0, 0}), {1, 0, -5}));   // world +x stays to the camera's right
    ZTH_EXPECT_TRUE(near(pt(*v, {0, 1, 0}), {0, 1, -5}));
    const auto w = Mat4::look_at({3, 4, 5}, {3, 4, 0}, {0, 1, 0});
    ZTH_EXPECT_TRUE(w.has_value() && near(pt(*w, {3, 4, 5}), {0, 0, 0}) && near(pt(*w, {3, 4, 0}), {0, 0, -5}));
    const auto side = Mat4::look_at({5, 0, 0}, {0, 0, 0}, {0, 1, 0});   // looking along -x: world -z is to the camera's right
    ZTH_EXPECT_TRUE(side.has_value() && near(pt(*side, {5, 0, -1}), {1, 0, 0}));
    ZTH_EXPECT_FALSE(Mat4::look_at({1, 2, 3}, {1, 2, 3}, {0, 1, 0}).has_value());   // eye == target
    ZTH_EXPECT_FALSE(Mat4::look_at({0, 0, 0}, {0, 5, 0}, {0, 1, 0}).has_value());   // looking straight along up
    ZTH_EXPECT_FALSE(Mat4::look_at({0, 0, 0}, {0, 5, 0}, {0, -2, 0}).has_value());
    ZTH_EXPECT_FALSE(Mat4::look_at({0, 0, 0}, {0, 0, -1}, {0, 0, 0}).has_value());
}
ZTH_TEST("works", "perspective: the layout, the near and far planes map to -1 and +1, and the perspective divide") {
    const auto p = Mat4::perspective(kPi / 2, 1.0, 1.0, 3.0);
    ZTH_EXPECT_TRUE(p.has_value());
    ZTH_EXPECT_NEAR(p->m[0], 1.0, 1e-12);
    ZTH_EXPECT_NEAR(p->m[5], 1.0, 1e-12);
    ZTH_EXPECT_NEAR(p->m[10], -2.0, 1e-12);
    ZTH_EXPECT_NEAR(p->m[11], -1.0, 0);
    ZTH_EXPECT_NEAR(p->m[14], -3.0, 1e-12);
    ZTH_EXPECT_NEAR(p->m[15], 0.0, 0);
    ZTH_EXPECT_TRUE(near(pt(*p, {0, 0, -1}), {0, 0, -1}));   // near plane
    ZTH_EXPECT_TRUE(near(pt(*p, {0, 0, -3}), {0, 0, 1}));    // far plane
    ZTH_EXPECT_TRUE(near(pt(*p, {0, 0, -2}), {0, 0, 0.5}));  // the depth is not linear
    ZTH_EXPECT_TRUE(near(pt(*p, {2, -1, -2}), {1, -0.5, 0.5}));   // 90 degrees: x = -z is the right edge
    const auto wide = Mat4::perspective(kPi / 2, 2.0, 0.5, 10.0);
    ZTH_EXPECT_TRUE(wide.has_value() && near(pt(*wide, {2, 0, -1}), {1, 0, pt(*wide, {2, 0, -1}).z}, 1e-12));   // aspect 2: the right edge is at x = 2 * -z
    ZTH_EXPECT_FALSE(p->transform_point({1, 2, 0}).has_value());   // w = -z = 0
}
ZTH_TEST("works", "perspective and look_at refuse impossible arguments") {
    ZTH_EXPECT_FALSE(Mat4::perspective(0.0, 1, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(kPi, 1, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(-1, 1, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 0, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, -1, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 1, 0, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 1, -1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 1, 5, 5).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 1, 5, 2).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(kNan, 1, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, kNan, 1, 10).has_value());
    ZTH_EXPECT_FALSE(Mat4::perspective(1, 1, 1, kInf).has_value());
    ZTH_EXPECT_TRUE(Mat4::perspective(0.5, 1.5, 0.1, 1000).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "length and normalized neither overflow nor underflow, and treat NaN and infinity sensibly") {
    ZTH_EXPECT_NEAR(length({1e200, 1e200, 0}) / 1e200, std::sqrt(2.0), 1e-15);
    ZTH_EXPECT_NEAR(length({3e300, 4e300, 0}) / 1e300, 5.0, 1e-14);
    ZTH_EXPECT_NEAR(length({1e-200, 1e-200, 1e-200}) / 1e-200, std::sqrt(3.0), 1e-15);
    ZTH_EXPECT_NEAR(length({-1e-300, 0, 0}) / 1e-300, 1.0, 1e-15);
    const auto big = normalized({1e200, 1e200, 1e200});
    ZTH_EXPECT_TRUE(big.has_value());
    ZTH_EXPECT_NEAR(length(*big), 1.0, 1e-14);
    ZTH_EXPECT_NEAR(big->x, 1 / std::sqrt(3.0), 1e-14);
    const auto tiny = normalized({1e-200, 0, 2e-200});
    ZTH_EXPECT_TRUE(tiny.has_value());
    ZTH_EXPECT_NEAR(tiny->z, 2 / std::sqrt(5.0), 1e-14);
    ZTH_EXPECT_TRUE(std::isnan(length({kNan, 1, 1})) && std::isnan(length({1, 1, kNan})));
    ZTH_EXPECT_TRUE(std::isnan(length({0, kNan, 0})) && std::isnan(length({0, 0, kNan})) && std::isnan(length({kNan, 0, 0})));   // a NaN next to zeros
    ZTH_EXPECT_TRUE(std::isinf(length({kInf, 1, 1})) && std::isinf(length({1, -kInf, 1})));
    ZTH_EXPECT_FALSE(normalized({kNan, 1, 1}).has_value());
    ZTH_EXPECT_FALSE(normalized({kInf, 1, 1}).has_value());
    ZTH_EXPECT_FALSE(normalized({0, -0.0, 0}).has_value());
}
ZTH_TEST("robust", "angle_between is accurate for tiny angles and never NaN") {
    for (double e : {1e-3, 1e-6, 1e-9, 1e-12}) {
        const double a = val(angle_between({1, 0, 0}, {1, e, 0}));
        ZTH_EXPECT_NEAR(a / std::atan(e), 1.0, 1e-12);   // acos would return 0 (or 1.49e-8) here
        ZTH_EXPECT_NEAR(val(angle_between({1, 0, 0}, {-1, e, 0})), kPi - std::atan(e), 2e-15);
    }
    zth::Lcg rng(5);
    for (int i = 0; i < 2000; ++i) {
        const Vec3 a = rv(rng), b = a * (0.5 + rng.below(100)) + rv(rng, 1e-9);
        if (!(length(a) > 1e-3)) continue;
        const double t = val(angle_between(a, b));
        ZTH_EXPECT_TRUE(t >= 0.0 && t <= kPi);
    }
    ZTH_EXPECT_NEAR(val(angle_between({1e200, 0, 0}, {0, 1e200, 0})), kPi / 2, 1e-12);
}
ZTH_TEST("robust", "algebra on random transforms: associativity, det(AB) = det(A) det(B), (AB)^-1 = B^-1 A^-1, A * A^-1 = I") {
    zth::Lcg rng(77);
    for (int round = 0; round < 300; ++round) {
        const Mat4 a = random_transform(rng), b = random_transform(rng), c = random_transform(rng);
        ZTH_EXPECT_TRUE(near((a * b) * c, a * (b * c), 1e-9));
        ZTH_EXPECT_NEAR((a * b).determinant(), a.determinant() * b.determinant(), 1e-8 * std::fabs(a.determinant() * b.determinant()) + 1e-10);
        const auto ia = a.inverse(), ib = b.inverse(), iab = (a * b).inverse();
        ZTH_EXPECT_TRUE(ia && ib && iab);
        if (!(ia && ib && iab)) return;
        ZTH_EXPECT_TRUE(near(a * *ia, Mat4::identity(), 1e-9));
        ZTH_EXPECT_TRUE(near(*ia * a, Mat4::identity(), 1e-9));
        ZTH_EXPECT_TRUE(near(*iab, *ib * *ia, 1e-8));
        const Vec3 p = rv(rng);
        ZTH_EXPECT_TRUE(near(pt(*ia, pt(a, p)), p, 1e-9));
        ZTH_EXPECT_TRUE(near(pt(a * b, p), pt(a, pt(b, p)), 1e-9));
    }
}
ZTH_TEST("robust", "rotations are orthonormal with determinant 1, preserve lengths and angles, and compose by adding angles") {
    zth::Lcg rng(9);
    for (int round = 0; round < 300; ++round) {
        Vec3 axis = rv(rng);
        if (!(length(axis) > 0.05)) axis = {1, 2, 3};
        const double a1 = (rng.below(6283) - 3141) / 1000.0, a2 = (rng.below(6283) - 3141) / 1000.0;
        const Mat4 r1 = *Mat4::rotation(axis, a1), r2 = *Mat4::rotation(axis, a2);
        ZTH_EXPECT_NEAR(r1.determinant(), 1.0, 1e-12);
        ZTH_EXPECT_TRUE(near(r1 * r1.transposed(), Mat4::identity(), 1e-12));
        ZTH_EXPECT_TRUE(near(r1 * r2, *Mat4::rotation(axis, a1 + a2), 1e-12));
        const Vec3 u = rv(rng), v = rv(rng);
        ZTH_EXPECT_NEAR(length(r1.transform_direction(u)), length(u), 1e-12);
        ZTH_EXPECT_NEAR(dot(r1.transform_direction(u), r1.transform_direction(v)), dot(u, v), 1e-11);
        ZTH_EXPECT_TRUE(near(r1.transform_direction(axis), axis, 1e-11));   // the axis is fixed
        ZTH_EXPECT_NEAR(length(reflect(u, *normalized(axis))), length(u), 1e-12);
    }
}
ZTH_TEST("robust", "look_at is a rigid motion that puts the eye at the origin and the target on the -z axis, for random cameras") {
    zth::Lcg rng(31);
    for (int round = 0; round < 300; ++round) {
        const Vec3 eye = rv(rng), target = rv(rng);
        const auto v = Mat4::look_at(eye, target, {0, 1, 0});
        if (!v) {
            ZTH_EXPECT_TRUE(length(target - eye) < 1e-3 || std::fabs(cross(target - eye, {0, 1, 0}).x) + std::fabs(cross(target - eye, {0, 1, 0}).z) < 1e-3);
            continue;
        }
        ZTH_EXPECT_TRUE(near(pt(*v, eye), {0, 0, 0}, 1e-9));
        const Vec3 t = pt(*v, target);
        ZTH_EXPECT_NEAR(t.x, 0.0, 1e-9);
        ZTH_EXPECT_NEAR(t.y, 0.0, 1e-9);
        ZTH_EXPECT_NEAR(t.z, -length(target - eye), 1e-9);
        ZTH_EXPECT_NEAR(v->determinant(), 1.0, 1e-9);
        // "up" stays up: a point above the eye along the world up, projected, has y >= 0
        ZTH_EXPECT_TRUE(pt(*v, eye + Vec3{0, 1, 0}).y >= -1e-12);
    }
}
ZTH_TEST("robust", "perspective maps the near plane to -1 and the far plane to +1 for random parameters, and depth increases with distance") {
    zth::Lcg rng(4);
    for (int round = 0; round < 300; ++round) {
        const double fov = 0.1 + rng.below(280) / 100.0, aspect = 0.3 + rng.below(300) / 100.0, n = 0.01 + rng.below(100) / 10.0, f = n + 0.5 + rng.below(1000);
        const auto p = Mat4::perspective(fov, aspect, n, f);
        ZTH_EXPECT_TRUE(p.has_value());
        if (!p) return;
        ZTH_EXPECT_NEAR(pt(*p, {0, 0, -n}).z, -1.0, 1e-9);
        ZTH_EXPECT_NEAR(pt(*p, {0, 0, -f}).z, 1.0, 1e-9);
        const double half_h = n * std::tan(fov / 2);
        ZTH_EXPECT_NEAR(pt(*p, {0, half_h, -n}).y, 1.0, 1e-9);
        ZTH_EXPECT_NEAR(pt(*p, {half_h * aspect, 0, -n}).x, 1.0, 1e-9);
        double last = -2;
        for (int i = 0; i <= 10; ++i) {
            const double z = pt(*p, {0, 0, -(n + (f - n) * i / 10.0)}).z;
            ZTH_EXPECT_TRUE(z > last);
            last = z;
        }
        const auto inv = p->inverse();
        ZTH_EXPECT_TRUE(inv.has_value());
        if (inv) ZTH_EXPECT_TRUE(near(pt(*inv, pt(*p, {0.3, -0.2, -(n + f) / 2})), {0.3, -0.2, -(n + f) / 2}, 1e-6 * (1 + f)));
    }
}
ZTH_TEST("robust", "transform_point: w = 0 and non-finite w are refused; directions ignore translation and projection") {
    Mat4 m = Mat4::identity();
    m.at(3, 3) = 0;
    ZTH_EXPECT_FALSE(m.transform_point({1, 2, 3}).has_value());
    m.at(3, 3) = kNan;
    ZTH_EXPECT_FALSE(m.transform_point({1, 2, 3}).has_value());
    m.at(3, 3) = kInf;
    ZTH_EXPECT_FALSE(m.transform_point({1, 2, 3}).has_value());
    const auto p = Mat4::perspective(1.0, 1.0, 1.0, 10.0);
    ZTH_EXPECT_TRUE(p.has_value() && near(p->transform_direction({0, 0, 1}), {0, 0, p->at(2, 2)}, 1e-12));
    ZTH_EXPECT_TRUE(near((Mat4::translation({9, 9, 9}) * Mat4::scaling({2, 2, 2})).transform_direction({1, 0, 0}), {2, 0, 0}, 1e-15));
}
ZTH_TEST("robust", "inverse of tiny, huge and badly scaled (but invertible) matrices, and of exactly singular ones") {
    const Mat4 tiny = Mat4::scaling({1e-30, 1e-30, 1e-30});
    const auto it = tiny.inverse();
    ZTH_EXPECT_TRUE(it.has_value() && std::fabs(it->at(0, 0) / 1e30 - 1.0) < 1e-12);
    const Mat4 huge = Mat4::scaling({1e30, 2e30, 4e30});
    const auto ih = huge.inverse();
    ZTH_EXPECT_TRUE(ih.has_value() && std::fabs(ih->at(2, 2) * 4e30 - 1.0) < 1e-12);
    Mat4 same_rows = Mat4::identity();
    for (std::size_t c = 0; c < 4; ++c) same_rows.at(1, c) = same_rows.at(0, c);
    ZTH_EXPECT_FALSE(same_rows.inverse().has_value());
    Mat4 overflow = Mat4::scaling({1e200, 1e200, 1e200});
    ZTH_EXPECT_FALSE(overflow.inverse().has_value());   // the determinant overflows to infinity
    Mat4 nan_m = Mat4::identity();
    nan_m.at(2, 1) = kNan;
    ZTH_EXPECT_FALSE(nan_m.inverse().has_value());
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "vectors and matrices never allocate") {
    zth::Lcg rng(1);
    Mat4 a = random_transform(rng);
    const Mat4 b = random_transform(rng);
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 2000; ++i) {
        a = a * b;
        const auto inv = b.inverse();
        const auto v = Mat4::look_at({1.0 * i, 2, 3}, {0, 0, 0}, {0, 1, 0});
        const auto p = Mat4::perspective(1.0, 1.5, 0.1, 100.0);
        sink += length(Vec3{1.0 * i, 2, 3}) + (inv ? inv->m[3] : 0) + (v ? v->m[1] : 0) + (p ? p->m[0] : 0) + val(angle_between({1, 2, 3}, {3, 2, 1}));
        a = Mat4::scaling({1, 1, 1}) * Mat4::translation({0.001, 0, 0});
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sink > 0);
}
ZTH_TEST_TIMEOUT("fast", "a million matrix products and a hundred thousand inverses", 20) {
    zth::Lcg rng(3);
    const Mat4 a = random_transform(rng), b = random_transform(rng);
    Mat4 acc = Mat4::identity();
    for (int i = 0; i < 1000000; ++i) {
        acc = a * b;
        acc.m[0] += 1e-9;
    }
    double s = 0;
    for (int i = 0; i < 100000; ++i) {
        Mat4 m = a;
        m.m[0] += i * 1e-6;
        const auto inv = m.inverse();
        s += inv ? inv->m[5] : 0;
    }
    ZTH_EXPECT_TRUE(std::isfinite(s) && std::isfinite(acc.m[0]));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no heap containers") {
    ZTH_EXPECT_NO_RAW_MEMORY("mathlib.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("mathlib.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "vector", "new", "function", "virtual"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "angles with atan2, not acos; lengths with the scaled formula, not hypot-by-squares; no pow for squares") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("mathlib.hpp"));
    for (const char* word : {"acos", "asin", "pow", "sqrtf", "float"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "atan2").empty());
}
