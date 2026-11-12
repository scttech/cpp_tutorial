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
inline Quat operator*(Quat a, Quat) { return a; }   // TODO
inline Quat conjugate(Quat q) { return q; }
inline double norm(Quat) { return 0; }
inline std::optional<Quat> normalized(Quat) { return std::nullopt; }
inline std::optional<Quat> inverse(Quat) { return std::nullopt; }
inline std::optional<Quat> from_axis_angle(Vec3, double) { return std::nullopt; }
inline Vec3 rotate(Quat, Vec3 v) { return v; }
inline Mat3 to_matrix(Quat) { return Mat3{1, 0, 0, 0, 1, 0, 0, 0, 1}; }
inline std::optional<Quat> from_matrix(const Mat3&) { return std::nullopt; }
inline double angle_between(Quat, Quat) { return 0; }
inline Quat slerp(Quat a, Quat, double) { return a; }

}  // namespace gfx
