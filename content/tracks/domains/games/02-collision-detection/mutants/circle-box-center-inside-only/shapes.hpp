// MUTANT: a circle overlaps a box only when its centre is inside
#pragma once
// Exact overlap tests (touching counts), all without a square root.
#include <algorithm>

#include "types.hpp"

namespace col {

inline bool overlaps(const Aabb& a, const Aabb& b) {
    return a.min.x <= b.max.x && b.min.x <= a.max.x && a.min.y <= b.max.y && b.min.y <= a.max.y;
}

inline bool overlaps(const Circle& a, const Circle& b) {
    const double reach = a.radius + b.radius;
    return length_squared(a.center - b.center) <= reach * reach;
}

inline bool overlaps(const Circle& c, const Aabb& box) {
    return c.center.x >= box.min.x && c.center.x <= box.max.x && c.center.y >= box.min.y && c.center.y <= box.max.y;
}

}  // namespace col
