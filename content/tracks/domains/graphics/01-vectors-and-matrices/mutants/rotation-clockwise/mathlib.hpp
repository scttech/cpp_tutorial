// MUTANT: the rotation is clockwise
#pragma once
// YOUR VECTOR AND MATRIX MATH (right-handed, like OpenGL: x right, y up, the camera looks down -z; clip space z in [-1, 1]).
//
// gfx::Vec3 { double x, y, z }   operators + - (binary and unary), * and / by a scalar (and scalar * Vec3), ==
//   dot(a, b), cross(a, b)
//   length(v)           must not overflow or underflow: (1e200, 1e200, 0) has length 1.414...e200, (1e-200, 0, 0) has length 1e-200. Scale by the largest |component| before squaring.
//                       NaN in -> NaN out; an infinite component -> infinity.
//   normalized(v)       optional<Vec3>: nullopt if the length is 0, NaN or infinite. Works for 1e200 and 1e-200 vectors too.
//   lerp(a, b, t)       a * (1 - t) + b * t
//   reflect(v, n)       v - 2 (v . n) n   (n is a unit normal)
//   angle_between(a, b) optional<double> in [0, pi]; nullopt if either vector has no length. Use atan2(|a x b|, a . b): acos(a . b / (|a| |b|)) is wildly inaccurate
//                       for nearly parallel vectors (and gives NaN when rounding pushes the ratio past 1).
//
// gfx::Mat4 { std::array<double, 16> m }   COLUMN-MAJOR: element (row r, column c) is m[c * 4 + r], so a translation sits in m[12], m[13], m[14]. at(r, c) reads/writes it.
//   Mat4::identity(), translation(t), scaling(s)
//   Mat4::rotation(axis, radians) -> optional<Mat4>     counter-clockwise (right-hand rule) about the axis, which need not be a unit vector; nullopt if it has no length
//   Mat4::look_at(eye, target, up) -> optional<Mat4>    the VIEW matrix: maps eye to the origin and the direction to the target to -z. nullopt if eye == target or up is parallel to the view direction
//   Mat4::perspective(fov_y, aspect, near_z, far_z) -> optional<Mat4>   fov_y in (0, pi) radians; nullopt unless aspect > 0 and 0 < near_z < far_z. Maps z = -near to -1 and z = -far to +1.
//   operator*(a, b)     the matrix product, so (a * b) transforms by b FIRST, then a
//   transposed(), determinant(), inverse() -> optional<Mat4> (nullopt when the determinant is exactly 0 or not finite; use cofactors, the general 4x4 inverse)
//   transform_point(p) -> optional<Vec3>      (x, y, z, 1) times the matrix, then DIVIDED by w; nullopt if w is 0 or not finite
//   transform_direction(d) -> Vec3            (x, y, z, 0): rotation and scale, no translation, no division
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace gfx {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(double s, Vec3 a) { return a * s; }
inline Vec3 operator/(Vec3 a, double s) { return {a.x / s, a.y / s, a.z / s}; }
inline bool operator==(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

inline double length(Vec3 v) {
    if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z)) return std::nan("");
    const double m = std::max({std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)});
    if (m == 0.0 || std::isinf(m)) return m;
    const double x = v.x / m, y = v.y / m, z = v.z / m;   // scaled so that the squares can neither overflow nor underflow
    return m * std::sqrt(x * x + y * y + z * z);
}

inline std::optional<Vec3> normalized(Vec3 v) {
    const double len = length(v);
    if (!(len > 0.0) || !std::isfinite(len)) return std::nullopt;
    return v / len;
}

inline Vec3 lerp(Vec3 a, Vec3 b, double t) { return a * (1.0 - t) + b * t; }
inline Vec3 reflect(Vec3 v, Vec3 n) { return v - n * (2.0 * dot(v, n)); }

inline std::optional<double> angle_between(Vec3 a, Vec3 b) {
    if (!(length(a) > 0.0) || !(length(b) > 0.0)) return std::nullopt;
    return std::atan2(length(cross(a, b)), dot(a, b));
}

struct Mat4 {
    std::array<double, 16> m{};
    double& at(std::size_t r, std::size_t c) { return m[c * 4 + r]; }
    double at(std::size_t r, std::size_t c) const { return m[c * 4 + r]; }

    static Mat4 identity() {
        Mat4 r;
        for (std::size_t i = 0; i < 4; ++i) r.at(i, i) = 1.0;
        return r;
    }
    static Mat4 translation(Vec3 t) {
        Mat4 r = identity();
        r.at(0, 3) = t.x;
        r.at(1, 3) = t.y;
        r.at(2, 3) = t.z;
        return r;
    }
    static Mat4 scaling(Vec3 s) {
        Mat4 r = identity();
        r.at(0, 0) = s.x;
        r.at(1, 1) = s.y;
        r.at(2, 2) = s.z;
        return r;
    }
    static std::optional<Mat4> rotation(Vec3 axis, double radians) {
        const auto u = normalized(axis);
        if (!u) return std::nullopt;
        const double c = std::cos(radians), s = -std::sin(radians), t = 1.0 - c;
        Mat4 r = identity();
        r.at(0, 0) = t * u->x * u->x + c;
        r.at(0, 1) = t * u->x * u->y - s * u->z;
        r.at(0, 2) = t * u->x * u->z + s * u->y;
        r.at(1, 0) = t * u->x * u->y + s * u->z;
        r.at(1, 1) = t * u->y * u->y + c;
        r.at(1, 2) = t * u->y * u->z - s * u->x;
        r.at(2, 0) = t * u->x * u->z - s * u->y;
        r.at(2, 1) = t * u->y * u->z + s * u->x;
        r.at(2, 2) = t * u->z * u->z + c;
        return r;
    }
    static std::optional<Mat4> look_at(Vec3 eye, Vec3 target, Vec3 up) {
        const auto f = normalized(target - eye);
        if (!f) return std::nullopt;
        const auto s = normalized(cross(*f, up));
        if (!s) return std::nullopt;
        const Vec3 u = cross(*s, *f);
        Mat4 r = identity();
        r.at(0, 0) = s->x; r.at(0, 1) = s->y; r.at(0, 2) = s->z; r.at(0, 3) = -dot(*s, eye);
        r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -dot(u, eye);
        r.at(2, 0) = -f->x; r.at(2, 1) = -f->y; r.at(2, 2) = -f->z; r.at(2, 3) = dot(*f, eye);
        return r;
    }
    static std::optional<Mat4> perspective(double fov_y, double aspect, double near_z, double far_z) {
        if (!(fov_y > 0.0 && fov_y < 3.14159265358979323846) || !(aspect > 0.0) || !(near_z > 0.0) || !(far_z > near_z) || !std::isfinite(far_z)) return std::nullopt;
        const double f = 1.0 / std::tan(fov_y / 2.0);
        Mat4 r;
        r.at(0, 0) = f / aspect;
        r.at(1, 1) = f;
        r.at(2, 2) = (far_z + near_z) / (near_z - far_z);
        r.at(2, 3) = 2.0 * far_z * near_z / (near_z - far_z);
        r.at(3, 2) = -1.0;
        return r;
    }

    Mat4 transposed() const {
        Mat4 r;
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j) r.at(i, j) = at(j, i);
        return r;
    }

    // the cofactor of (r, c): the signed determinant of the 3x3 left when row r and column c are removed
    double cofactor(std::size_t r, std::size_t c) const {
        double a[3][3];
        for (std::size_t i = 0, ri = 0; i < 4; ++i) {
            if (i == r) continue;
            for (std::size_t j = 0, cj = 0; j < 4; ++j) {
                if (j == c) continue;
                a[ri][cj++] = at(i, j);
            }
            ++ri;
        }
        const double d = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
        return ((r + c) % 2 == 0) ? d : -d;
    }
    double determinant() const {
        double d = 0.0;
        for (std::size_t c = 0; c < 4; ++c) d += at(0, c) * cofactor(0, c);
        return d;
    }
    std::optional<Mat4> inverse() const {
        const double det = determinant();
        if (det == 0.0 || !std::isfinite(det)) return std::nullopt;
        Mat4 r;
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j) r.at(i, j) = cofactor(j, i) / det;   // the adjugate is the transposed cofactor matrix
        for (double v : r.m)
            if (!std::isfinite(v)) return std::nullopt;
        return r;
    }

    std::optional<Vec3> transform_point(Vec3 p) const {
        const double x = at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3);
        const double y = at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3);
        const double z = at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3);
        const double w = at(3, 0) * p.x + at(3, 1) * p.y + at(3, 2) * p.z + at(3, 3);
        if (w == 0.0 || !std::isfinite(w)) return std::nullopt;
        return Vec3{x / w, y / w, z / w};
    }
    Vec3 transform_direction(Vec3 d) const {
        return {at(0, 0) * d.x + at(0, 1) * d.y + at(0, 2) * d.z, at(1, 0) * d.x + at(1, 1) * d.y + at(1, 2) * d.z, at(2, 0) * d.x + at(2, 1) * d.y + at(2, 2) * d.z};
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < 4; ++k) s += a.at(i, k) * b.at(k, j);
            r.at(i, j) = s;
        }
    return r;
}

}  // namespace gfx
