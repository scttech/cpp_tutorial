#pragma once
// YOUR PURE-PURSUIT FOLLOWER (namespace robot; Vec2 and Pose are in types.hpp). Positions are in metres, angles in radians, theta = 0 means facing +x.
//
// wrap_angle(a)                    a in (-pi, pi] (so -pi becomes +pi). Exact for huge arguments (use fmod, not a loop). NaN and infinities give NaN.
//
// advance(pose, v, kappa, dt) -> Pose      move for dt seconds at speed v with CURVATURE kappa (1 / turning radius; positive = counter-clockwise), along the exact circular arc, not Euler steps:
//        arc length d = v dt, turn angle delta = kappa d, chord = d * sinc(delta / 2) heading theta + delta / 2:
//        x += chord cos(theta + delta / 2),  y += chord sin(theta + delta / 2),  theta = wrap_angle(theta + delta).
//        sinc(u) = sin(u) / u, and 1 at u = 0 (so kappa = 0 and kappa = 1e-12 are both straight lines). The tempting alternative, x += (sin(theta + delta) - sin(theta)) / kappa, subtracts two nearly equal
//        numbers and is noise for tiny kappa. If the pose or an argument is not finite, or dt < 0: the pose is returned unchanged.
//
// Roots circle_segment(center, r, a, b)    the parameters t in [0, 1] (ascending) of the points a + t (b - a) at distance exactly r from center: Roots { int count; double t[2]; }.
//        Use the stable discriminant   |d|^2 r^2 - cross(a - center, d)^2   (d = b - a), t = (-(f.d) -/+ sqrt(disc)) / |d|^2 with f = a - center. A tangent line is ONE root. No roots when disc < 0, when a == b,
//        when r is not a finite number > 0, or when an input is not finite. Roots outside [0, 1] are dropped.
//
// Path                              a polyline.  Path::make(points) -> optional<Path>: nullopt if a point is not finite or fewer than two DISTINCT points remain; consecutive duplicates are removed first
//        (a zero-length segment would break every projection). size(), points(), segment_count() (= size() - 1), segment_length(i), total_length().
//
// Closest closest_on_path(path, p, first, last)   { size_t segment; double t; Vec2 point; double distance; }  the nearest point of the path among the segments first..last (inclusive; both clamped to the
//        valid range, last >= first). t in [0, 1] is the position on that segment. Ties go to the EARLIER segment.
//
// lookahead_point(path, robot, L, segment, t) -> optional<Vec2>      the "carrot": scan the segments from `segment` onwards, intersect each with the circle of radius L around the robot (circle_segment);
//        on the starting segment ignore roots before t (the robot has already passed them); the answer is the LARGEST valid root of the FIRST segment that has one. nullopt if there is none (the rest of the
//        path lies inside the circle, or the robot is more than L away from it).
//
// Params { double lookahead, max_speed, max_curvature, goal_tolerance }
// Command { double v; double kappa; bool done; }
// Follower::make(path, params) -> optional<Follower>      nullopt unless lookahead, max_speed and max_curvature are finite numbers > 0 and goal_tolerance is finite and >= 0
//   segment() -> the path segment the robot is currently on (it never decreases)
//   step(pose) -> Command:
//     * a pose that is not finite -> {0, 0, false}, nothing else changes.
//     * dg = distance to the last point of the path. If dg <= goal_tolerance (or it was reached before): {0, 0, true}, and the follower stays done.
//     * Find the nearest point on the path among the segments segment() .. segment() + 8, and set segment() to its segment.
//     * Carrot = lookahead_point(...) from that nearest point; if there is none: the last point of the path when dg < lookahead (the end is in sight), else the nearest point itself (come back to the path).
//     * Transform the carrot into the robot's frame (lx forward, ly to the left) and let d2 = lx^2 + ly^2.   kappa = 2 ly / d2   (the circle through the robot and the carrot that is tangent to the heading),
//       or 0 if d2 < 1e-12. If the carrot is behind the robot (lx < 0) turn as hard as possible towards the side it is on: kappa = +max_curvature if ly >= 0 else -max_curvature.
//       Then clamp |kappa| to max_curvature.
//     * Speed: v = max_speed * (1 - 0.8 |kappa| / max_curvature) * min(1, max(0.1, dg / lookahead)): slower in tight turns and near the goal, never zero and never above max_speed.
//     * Return {v, kappa, false}.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "types.hpp"

namespace robot {

struct Roots {
    int count = 0;
    double t[2] = {0.0, 0.0};
};

struct Closest {
    std::size_t segment = 0;
    double t = 0;
    Vec2 point;
    double distance = 0;
};

struct Params {
    double lookahead = 1.0, max_speed = 1.0, max_curvature = 1.0, goal_tolerance = 0.1;
};

struct Command {
    double v = 0;
    double kappa = 0;
    bool done = false;
};

inline double wrap_angle(double a) {
    if (!std::isfinite(a)) return std::nan("");
    double r = std::fmod(a, 2.0 * kPi);
    if (r > kPi) r -= 2.0 * kPi;
    else if (r <= -kPi) r += 2.0 * kPi;
    return r;
}

namespace detail {
inline double sinc(double u) { return u == 0.0 ? 1.0 : std::sin(u) / u; }
}  // namespace detail

inline Pose advance(Pose p, double v, double kappa, double dt) {
    if (!is_finite(p) || !std::isfinite(v) || !std::isfinite(kappa) || !std::isfinite(dt) || dt < 0.0) return p;
    const double d = v * dt;
    const double delta = kappa * d;
    const double chord = d * detail::sinc(delta / 2.0);
    const double mid = p.theta + delta / 2.0;
    return Pose{p.x + chord * std::cos(mid), p.y + chord * std::sin(mid), wrap_angle(p.theta + delta)};
}

inline Roots circle_segment(Vec2 center, double r, Vec2 a, Vec2 b) {
    Roots out;
    if (!is_finite(center) || !is_finite(a) || !is_finite(b) || !(r > 0.0) || !std::isfinite(r)) return out;
    const Vec2 d = b - a, f = a - center;
    const double A = dot(d, d);
    if (!(A > 0.0)) return out;
    const double c = cross(f, d);
    const double disc = A * r * r - c * c;
    if (disc < 0.0) return out;
    const double half_b = dot(f, d);
    const double root = std::sqrt(disc);
    const double lo = (-half_b - root) / A, hi = (-half_b + root) / A;
    if (lo >= 0.0 && lo <= 1.0) out.t[out.count++] = lo;
    if (hi >= 0.0 && hi <= 1.0 && (disc > 0.0)) out.t[out.count++] = hi;
    return out;
}

class Path {
public:
    static std::optional<Path> make(std::vector<Vec2> pts) {
        std::vector<Vec2> clean;
        for (const Vec2& p : pts) {
            if (!is_finite(p)) return std::nullopt;
            if (clean.empty() || p.x != clean.back().x || p.y != clean.back().y) clean.push_back(p);
        }
        if (clean.size() < 2) return std::nullopt;
        Path path;
        path.pts_ = std::move(clean);
        return path;
    }
    std::size_t size() const { return pts_.size(); }
    const std::vector<Vec2>& points() const { return pts_; }
    std::size_t segment_count() const { return pts_.size() > 0 ? pts_.size() - 1 : 0; }
    double segment_length(std::size_t i) const { return distance(pts_[i], pts_[i + 1]); }
    double total_length() const {
        double s = 0.0;
        for (std::size_t i = 0; i + 1 < pts_.size(); ++i) s += segment_length(i);
        return s;
    }

private:
    std::vector<Vec2> pts_;
};

inline Closest closest_on_path(const Path& path, Vec2 p, std::size_t first, std::size_t last) {
    const std::size_t n = path.segment_count();
    first = std::min(first, n - 1);
    last = std::min(std::max(last, first), n - 1);
    Closest best;
    best.distance = std::numeric_limits<double>::infinity();
    for (std::size_t i = first; i <= last; ++i) {
        const Vec2 a = path.points()[i], b = path.points()[i + 1];
        const Vec2 d = b - a;
        const double t = std::clamp(dot(p - a, d) / dot(d, d), 0.0, 1.0);
        const Vec2 q = a + d * t;
        const double dist = distance(p, q);
        if (dist < best.distance) best = Closest{i, t, q, dist};
    }
    return best;
}

inline std::optional<Vec2> lookahead_point(const Path& path, Vec2 robot, double L, std::size_t segment, double t) {
    for (std::size_t i = segment; i < path.segment_count(); ++i) {
        const Vec2 a = path.points()[i], b = path.points()[i + 1];
        const Roots r = circle_segment(robot, L, a, b);
        for (int k = r.count - 1; k >= 0; --k)
            if (i != segment || r.t[k] >= t) return a + (b - a) * r.t[k];
    }
    return std::nullopt;
}

class Follower {
public:
    static std::optional<Follower> make(Path path, Params params) {
        const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
        if (!positive(params.lookahead) || !positive(params.max_speed) || !positive(params.max_curvature)) return std::nullopt;
        if (!std::isfinite(params.goal_tolerance) || params.goal_tolerance < 0.0) return std::nullopt;
        return Follower(std::move(path), params);
    }
    std::size_t segment() const { return segment_; }

    Command step(Pose pose) {
        if (!is_finite(pose)) return Command{};
        const Vec2 pos{pose.x, pose.y};
        const Vec2 goal = path_.points().back();
        const double dg = distance(pos, goal);
        if (done_ || dg <= params_.goal_tolerance) {
            done_ = true;
            return Command{0.0, 0.0, true};
        }
        const Closest c = closest_on_path(path_, pos, segment_, segment_ + 8);
        segment_ = c.segment;
        const auto carrot = lookahead_point(path_, pos, params_.lookahead, c.segment, c.t);
        const Vec2 target = carrot ? *carrot : (dg < params_.lookahead ? goal : c.point);
        const double dx = target.x - pose.x, dy = target.y - pose.y;
        const double lx = std::cos(pose.theta) * dx + std::sin(pose.theta) * dy;
        const double ly = -std::sin(pose.theta) * dx + std::cos(pose.theta) * dy;
        const double d2 = lx * lx + ly * ly;
        double kappa = d2 < 1e-12 ? 0.0 : 2.0 * ly / d2;
        if (lx < 0.0) kappa = ly >= 0.0 ? params_.max_curvature : -params_.max_curvature;
        kappa = std::clamp(kappa, -params_.max_curvature, params_.max_curvature);
        const double v = params_.max_speed * (1.0 - 0.8 * std::fabs(kappa) / params_.max_curvature) * std::min(1.0, std::max(0.1, dg / params_.lookahead));
        return Command{v, kappa, false};
    }

private:
    Follower(Path path, Params params) : path_(std::move(path)), params_(params) {}
    Path path_;
    Params params_;
    std::size_t segment_ = 0;
    bool done_ = false;
};

}  // namespace robot
