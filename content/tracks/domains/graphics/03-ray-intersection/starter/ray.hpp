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

// TODO: the four intersect() functions. These placeholders never hit anything.
inline std::optional<Hit> intersect(const Ray&, const Sphere&, double = 0.0, double = kInfinity) { return std::nullopt; }
inline std::optional<Hit> intersect(const Ray&, const Plane&, double = 0.0, double = kInfinity) { return std::nullopt; }
inline std::optional<Hit> intersect(const Ray&, const Aabb&, double = 0.0, double = kInfinity) { return std::nullopt; }
inline std::optional<Hit> intersect(const Ray&, const Triangle&, double = 0.0, double = kInfinity) { return std::nullopt; }

}  // namespace gfx
