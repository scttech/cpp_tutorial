#pragma once
// READ-ONLY. The geometry types.
#include <cmath>

namespace col {

struct Vec2 {
    double x = 0, y = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double length_squared(Vec2 a) { return dot(a, a); }

// an axis-aligned box: min.x <= max.x and min.y <= max.y (a box with min == max is a point)
struct Aabb {
    Vec2 min, max;
};

struct Circle {
    Vec2 center;
    double radius = 0;
};

}  // namespace col
