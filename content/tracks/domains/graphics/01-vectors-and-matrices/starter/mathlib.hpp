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

// TODO: operators for Vec3
inline Vec3 operator+(Vec3 a, Vec3 b) { return a; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return a; }
inline Vec3 operator-(Vec3 a) { return a; }
inline Vec3 operator*(Vec3 a, double) { return a; }
inline Vec3 operator*(double, Vec3 a) { return a; }
inline Vec3 operator/(Vec3 a, double) { return a; }
inline bool operator==(Vec3, Vec3) { return false; }

inline double dot(Vec3, Vec3) { return 0; }
inline Vec3 cross(Vec3 a, Vec3) { return a; }
inline double length(Vec3) { return 0; }
inline std::optional<Vec3> normalized(Vec3) { return std::nullopt; }
inline Vec3 lerp(Vec3 a, Vec3, double) { return a; }
inline Vec3 reflect(Vec3 v, Vec3) { return v; }
inline std::optional<double> angle_between(Vec3, Vec3) { return std::nullopt; }

struct Mat4 {
    std::array<double, 16> m{};
    double& at(std::size_t r, std::size_t c) { return m[c * 4 + r]; }
    double at(std::size_t r, std::size_t c) const { return m[c * 4 + r]; }

    // TODO
    static Mat4 identity() { return Mat4{}; }
    static Mat4 translation(Vec3) { return Mat4{}; }
    static Mat4 scaling(Vec3) { return Mat4{}; }
    static std::optional<Mat4> rotation(Vec3, double) { return std::nullopt; }
    static std::optional<Mat4> look_at(Vec3, Vec3, Vec3) { return std::nullopt; }
    static std::optional<Mat4> perspective(double, double, double, double) { return std::nullopt; }
    Mat4 transposed() const { return *this; }
    double determinant() const { return 0; }
    std::optional<Mat4> inverse() const { return std::nullopt; }
    std::optional<Vec3> transform_point(Vec3) const { return std::nullopt; }
    Vec3 transform_direction(Vec3 d) const { return d; }
};
inline Mat4 operator*(const Mat4& a, const Mat4&) { return a; }

}  // namespace gfx
