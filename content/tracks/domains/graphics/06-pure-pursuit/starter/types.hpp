#pragma once
// Read-only helper types for the robotics lesson.
#include <cmath>

namespace robot {

inline constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x = 0, y = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }   // z component of the 3D cross product
inline double distance(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
inline bool is_finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }

// Where the robot is and which way it faces (theta in radians, 0 = along +x, counter-clockwise positive).
struct Pose {
    double x = 0, y = 0, theta = 0;
};
inline bool is_finite(Pose p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.theta); }

}  // namespace robot
