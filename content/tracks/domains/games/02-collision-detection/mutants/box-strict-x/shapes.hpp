// MUTANT: boxes that touch on x do not overlap
#pragma once
// Exact overlap tests (touching counts), all without a square root.
#include <algorithm>

#include "types.hpp"

namespace col {

inline bool overlaps(const Aabb& a, const Aabb& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y <= b.max.y && b.min.y <= a.max.y;
}

inline bool overlaps(const Circle& a, const Circle& b) {
    const double reach = a.radius + b.radius;
    return length_squared(a.center - b.center) <= reach * reach;
}

inline bool overlaps(const Circle& c, const Aabb& box) {
    const Vec2 closest{std::clamp(c.center.x, box.min.x, box.max.x), std::clamp(c.center.y, box.min.y, box.max.y)};
    return length_squared(c.center - closest) <= c.radius * c.radius;
}

}  // namespace col
