// MUTANT: the normal is not divided by the radius
#pragma once
// YOUR RAY INTERSECTION CODE.
//
// gfx::Ray { Vec3 origin, dir }      dir need NOT be a unit vector: the answer t is in units of dir, so the hit point is origin + t * dir
// gfx::Hit { double t; Vec3 point; Vec3 normal; bool front; }
//     point = origin + t * dir.   normal is a UNIT vector that always FACES THE RAY (dot(normal, dir) <= 0), so a shader can use it directly.
//     front is true when the ray meets the shape from the side its natural outward normal points to (outside of a sphere or box; the front of a plane, whose side is given by its normal;
//     the side of a triangle that (b - a) x (c - a) points to) and false for a back face (a ray starting inside a sphere or box, or arriving from behind a plane or triangle).
//
// Shapes: Sphere { center, radius }, Plane { point, normal } (normal need not be unit), Aabb { min, max } (an axis-aligned box), Triangle { a, b, c }.
//
// intersect(ray, shape, t_min = 0.0, t_max = infinity) -> optional<Hit>      for each of the four shapes
//     The NEAREST hit whose t is in the closed interval [t_min, t_max]. nullopt for a miss, and for invalid input:
//       * a ray whose origin or direction is not finite, or whose direction has a squared length that is zero or not finite;
//       * t_min or t_max NaN, or t_min > t_max;
//       * a sphere whose centre is not finite or whose radius is not a finite number > 0; a plane with a zero/non-finite normal or a non-finite point;
//         a box with a non-finite corner or min > max on some axis (min == max on an axis is fine: a flat box); a triangle with a non-finite corner or zero area.
//     Sphere:   use the discriminant  |dir|^2 r^2 - |(origin - center) x dir|^2  (it equals b^2 - a c but does not cancel when the sphere is far away). A tangent ray is a hit. If the near root is before
//               t_min, take the far root (a ray that starts inside the sphere hits the back of it).
//     Plane:    a ray parallel to the plane (|n . dir| <= 1e-12 |n| |dir|) never hits, even when it lies in the plane.
//     Aabb:     the slab method. An axis on which dir is exactly 0 needs its own branch (1 / 0 is infinity and 0 * infinity is NaN, which poisons min/max code): the ray misses unless its origin lies in that slab (inclusive).
//               A ray that starts inside the box hits the exit face (front = false). The normal is the normal of the face that was hit.
//     Triangle: Moller-Trumbore; double-sided; the edges and corners belong to the triangle. A ray parallel to the triangle's plane never hits.
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "types.hpp"

namespace gfx {

struct Ray {
    Vec3 origin, dir;
};
struct Hit {
    double t = 0;
    Vec3 point;
    Vec3 normal;
    bool front = true;
};
struct Sphere {
    Vec3 center;
    double radius;
};
struct Plane {
    Vec3 point, normal;
};
struct Aabb {
    Vec3 min, max;
};
struct Triangle {
    Vec3 a, b, c;
};

inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

namespace detail {

inline bool valid(const Ray& r, double t_min, double t_max) {
    if (!is_finite(r.origin) || !is_finite(r.dir)) return false;
    const double d2 = dot(r.dir, r.dir);
    if (!(d2 > 0.0) || !std::isfinite(d2)) return false;
    return !std::isnan(t_min) && !std::isnan(t_max) && t_min <= t_max;
}

// the hit record for parameter t on a surface whose outward unit normal there is `outward`
inline Hit make_hit(const Ray& r, double t, Vec3 outward) {
    Hit h;
    h.t = t;
    h.point = r.origin + r.dir * t;
    h.front = dot(r.dir, outward) < 0.0;
    h.normal = h.front ? outward : -outward;
    return h;
}

}  // namespace detail

inline std::optional<Hit> intersect(const Ray& ray, const Sphere& s, double t_min = 0.0, double t_max = kInfinity) {
    if (!detail::valid(ray, t_min, t_max) || !is_finite(s.center) || !(s.radius > 0.0) || !std::isfinite(s.radius)) return std::nullopt;
    const Vec3 oc = ray.origin - s.center;
    const double a = dot(ray.dir, ray.dir);
    const double half_b = dot(oc, ray.dir);
    const Vec3 perp = cross(oc, ray.dir);
    const double disc = a * s.radius * s.radius - dot(perp, perp);   // b^2 - a c, without the cancellation
    if (disc < 0.0) return std::nullopt;
    const double root = std::sqrt(disc);
    const double near_t = (-half_b - root) / a, far_t = (-half_b + root) / a;
    const double t = near_t >= t_min ? near_t : far_t;
    if (!(t >= t_min && t <= t_max)) return std::nullopt;
    return detail::make_hit(ray, t, oc + ray.dir * t);
}

inline std::optional<Hit> intersect(const Ray& ray, const Plane& p, double t_min = 0.0, double t_max = kInfinity) {
    if (!detail::valid(ray, t_min, t_max) || !is_finite(p.point) || !is_finite(p.normal)) return std::nullopt;
    const double nl = length(p.normal);
    if (!(nl > 0.0) || !std::isfinite(nl)) return std::nullopt;
    const double denom = dot(p.normal, ray.dir);
    if (std::fabs(denom) <= 1e-12 * nl * length(ray.dir)) return std::nullopt;
    const double t = dot(p.normal, p.point - ray.origin) / denom;
    if (!(t >= t_min && t <= t_max)) return std::nullopt;
    return detail::make_hit(ray, t, p.normal / nl);
}

inline std::optional<Hit> intersect(const Ray& ray, const Aabb& box, double t_min = 0.0, double t_max = kInfinity) {
    if (!detail::valid(ray, t_min, t_max) || !is_finite(box.min) || !is_finite(box.max)) return std::nullopt;
    const double o[3] = {ray.origin.x, ray.origin.y, ray.origin.z}, d[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const double lo[3] = {box.min.x, box.min.y, box.min.z}, hi[3] = {box.max.x, box.max.y, box.max.z};
    for (int i = 0; i < 3; ++i)
        if (lo[i] > hi[i]) return std::nullopt;
    double t_enter = -kInfinity, t_exit = kInfinity;
    int enter_axis = -1, exit_axis = -1;
    double enter_sign = 0, exit_sign = 0;   // outward normal components of the faces entered and left
    for (int i = 0; i < 3; ++i) {
        if (d[i] == 0.0) {
            if (o[i] < lo[i] || o[i] > hi[i]) return std::nullopt;   // parallel to this slab and outside it
            continue;
        }
        double t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
        const double face_in = d[i] > 0.0 ? -1.0 : 1.0;   // entering through the low face (outward -axis) when moving in +axis
        if (t1 > t2) std::swap(t1, t2);
        if (t1 > t_enter) {
            t_enter = t1;
            enter_axis = i;
            enter_sign = face_in;
        }
        if (t2 < t_exit) {
            t_exit = t2;
            exit_axis = i;
            exit_sign = -face_in;
        }
        if (t_enter > t_exit) return std::nullopt;
    }
    double t;
    int axis;
    double sign;
    if (t_enter >= t_min) {
        t = t_enter;
        axis = enter_axis;
        sign = enter_sign;
    } else {
        t = t_exit;
        axis = exit_axis;
        sign = exit_sign;
    }
    if (!(t >= t_min && t <= t_max)) return std::nullopt;
    Vec3 outward;
    (axis == 0 ? outward.x : axis == 1 ? outward.y : outward.z) = sign;
    return detail::make_hit(ray, t, outward);
}

inline std::optional<Hit> intersect(const Ray& ray, const Triangle& tri, double t_min = 0.0, double t_max = kInfinity) {
    if (!detail::valid(ray, t_min, t_max) || !is_finite(tri.a) || !is_finite(tri.b) || !is_finite(tri.c)) return std::nullopt;
    const Vec3 e1 = tri.b - tri.a, e2 = tri.c - tri.a;
    const Vec3 n = cross(e1, e2);
    const double nl = length(n);
    if (!(nl > 0.0) || !std::isfinite(nl)) return std::nullopt;
    const Vec3 p = cross(ray.dir, e2);
    const double det = dot(e1, p);
    if (std::fabs(det) <= 1e-12 * length(e1) * length(e2) * length(ray.dir)) return std::nullopt;
    const double inv = 1.0 / det;
    const Vec3 tv = ray.origin - tri.a;
    const double u = dot(tv, p) * inv;
    if (u < 0.0 || u > 1.0) return std::nullopt;
    const Vec3 q = cross(tv, e1);
    const double v = dot(ray.dir, q) * inv;
    if (v < 0.0 || u + v > 1.0) return std::nullopt;
    const double t = dot(e2, q) * inv;
    if (!(t >= t_min && t <= t_max)) return std::nullopt;
    return detail::make_hit(ray, t, n / nl);
}

}  // namespace gfx
