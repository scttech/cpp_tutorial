#pragma once
// YOUR QUATERNIONS. A unit quaternion q = (w, x, y, z) = (cos(a/2), sin(a/2) * axis) is a rotation by the angle a about the unit axis.
//
// Provided below (already written): the operators + and unary - and * by a double, dot(Quat, Quat).
//
// gfx::Quat { double w = 1, x = 0, y = 0, z = 0 }          the default is the identity rotation
//   operator*(a, b)            the Hamilton product. (a * b) rotates by b FIRST and then by a, exactly like matrices.
//   conjugate(q)               (w, -x, -y, -z): for a unit quaternion this is the inverse rotation
//   norm(q)                    sqrt(w^2 + x^2 + y^2 + z^2) without overflow or underflow (std::hypot)
//   normalized(q)              optional<Quat>: nullopt if the norm is 0, NaN or infinite. Divide the components by the norm (do not multiply by 1/norm: it can overflow).
//   inverse(q)                 optional<Quat>: conjugate(q) / norm(q)^2, for any non-zero quaternion (not only unit ones); nullopt for zero/non-finite. Do not compute norm^2 directly.
//   from_axis_angle(axis, radians) -> optional<Quat>      the axis need not be a unit vector; nullopt if it has no length. Uses the HALF angle.
//   rotate(q, v) -> Vec3       rotate v by the UNIT quaternion q without building the matrix: t = 2 (u x v), v' = v + w t + u x t   (u = (x, y, z))
//   to_matrix(q) -> Mat3       the 3x3 rotation matrix of the unit quaternion q (row-major, acting on column vectors)
//   from_matrix(m) -> optional<Quat>   the unit quaternion of a rotation matrix, with w >= 0 (the canonical sign). nullopt if m is not finite, not orthonormal (m * m^T differs from I by
//                              more than 1e-6 in any element) or has a negative determinant (a mirror image). Must work for every rotation, in particular for angles near 180 degrees where
//                              w is near 0: use Shepperd's method (start from the largest of w, x, y, z) instead of dividing by w.
//   angle_between(a, b)        the smallest angle (0 .. pi) that turns orientation a into orientation b: 2 * atan2(|vector part of conjugate(a) * b|, |w of conjugate(a) * b|).
//                              q and -q are the same orientation, so angle_between(q, -q) = 0. Accurate for tiny angles (acos is not).
//   slerp(a, b, t)             spherical linear interpolation between UNIT quaternions, along the SHORTEST arc (if dot(a, b) < 0 use -b), at constant angular speed:
//                              sin((1 - t) theta) / sin(theta) * a + sin(t theta) / sin(theta) * b, theta = acos(dot). When a and b are almost equal (dot > 0.9995) sin(theta) is
//                              too small to divide by: use the normalised linear interpolation instead. Clamp dot to [-1, 1] before acos. The result is a unit quaternion.
#include <algorithm>
#include <cmath>
#include <optional>

#include "types.hpp"

namespace gfx {

struct Quat {
    double w = 1, x = 0, y = 0, z = 0;
};

// ---- provided ----
inline Quat operator+(Quat a, Quat b) { return {a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Quat operator-(Quat a) { return {-a.w, -a.x, -a.y, -a.z}; }
inline Quat operator*(Quat a, double s) { return {a.w * s, a.x * s, a.y * s, a.z * s}; }
inline double dot(Quat a, Quat b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }

// ---- yours ----
inline Quat operator*(Quat a, Quat b) {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
inline Quat conjugate(Quat q) { return {q.w, -q.x, -q.y, -q.z}; }
inline double norm(Quat q) { return std::hypot(std::hypot(q.w, q.x), std::hypot(q.y, q.z)); }

inline std::optional<Quat> normalized(Quat q) {
    const double n = norm(q);
    if (!(n > 0.0) || !std::isfinite(n)) return std::nullopt;
    return Quat{q.w / n, q.x / n, q.y / n, q.z / n};
}
inline std::optional<Quat> inverse(Quat q) {
    const double n = norm(q);
    if (!(n > 0.0) || !std::isfinite(n)) return std::nullopt;
    return Quat{q.w / n / n, -q.x / n / n, -q.y / n / n, -q.z / n / n};
}

inline std::optional<Quat> from_axis_angle(Vec3 axis, double radians) {
    const double len = length(axis);
    if (!(len > 0.0) || !std::isfinite(len)) return std::nullopt;
    const double s = std::sin(radians / 2.0);
    return Quat{std::cos(radians / 2.0), axis.x / len * s, axis.y / len * s, axis.z / len * s};
}

inline Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = cross(u, v) * 2.0;
    return v + t * q.w + cross(u, t);
}

inline Mat3 to_matrix(Quat q) {
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return Mat3{1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy),
                2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx),
                2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)};
}

inline std::optional<Quat> from_matrix(const Mat3& m) {
    for (double v : m)
        if (!std::isfinite(v)) return std::nullopt;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += m[static_cast<std::size_t>(i * 3 + k)] * m[static_cast<std::size_t>(j * 3 + k)];
            if (std::fabs(s - (i == j ? 1.0 : 0.0)) > 1e-6) return std::nullopt;
        }
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (det < 0.0) return std::nullopt;
    Quat q;
    const double tr = m[0] + m[4] + m[8];
    if (tr > 0.0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;   // s = 4 w
        q = {s / 4.0, (m[7] - m[5]) / s, (m[2] - m[6]) / s, (m[3] - m[1]) / s};
    } else if (m[0] > m[4] && m[0] > m[8]) {
        const double s = std::sqrt(1.0 + m[0] - m[4] - m[8]) * 2.0;   // s = 4 x
        q = {(m[7] - m[5]) / s, s / 4.0, (m[1] + m[3]) / s, (m[2] + m[6]) / s};
    } else if (m[4] > m[8]) {
        const double s = std::sqrt(1.0 + m[4] - m[0] - m[8]) * 2.0;   // s = 4 y
        q = {(m[2] - m[6]) / s, (m[1] + m[3]) / s, s / 4.0, (m[5] + m[7]) / s};
    } else {
        const double s = std::sqrt(1.0 + m[8] - m[0] - m[4]) * 2.0;   // s = 4 z
        q = {(m[3] - m[1]) / s, (m[2] + m[6]) / s, (m[5] + m[7]) / s, s / 4.0};
    }
    const auto n = normalized(q);
    if (!n) return std::nullopt;
    return n->w < 0.0 ? -*n : *n;
}

inline double angle_between(Quat a, Quat b) {
    const Quat r = conjugate(a) * b;
    return 2.0 * std::atan2(length(Vec3{r.x, r.y, r.z}), std::fabs(r.w));
}

inline Quat slerp(Quat a, Quat b, double t) {
    double d = dot(a, b);
    if (d < 0.0) {   // the long way round: use the other representation of b
        b = -b;
        d = -d;
    }
    d = std::min(d, 1.0);
    if (d > 0.9995) {
        const Quat l = a * (1.0 - t) + b * t;
        return normalized(l).value_or(a);
    }
    const double theta = std::acos(d);
    const double s = std::sin(theta);
    return a * (std::sin((1.0 - t) * theta) / s) + b * (std::sin(t * theta) / s);
}

}  // namespace gfx
