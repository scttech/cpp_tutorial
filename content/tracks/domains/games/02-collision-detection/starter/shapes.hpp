#pragma once
// YOUR NARROW PHASE: exact overlap tests. Shapes that merely TOUCH (share a boundary point) overlap.
//   overlaps(Aabb, Aabb)      the intervals overlap on both axes
//   overlaps(Circle, Circle)  the distance between the centres is at most the sum of the radii (compare SQUARED distances: no sqrt)
//   overlaps(Circle, Aabb)    the point of the box closest to the centre is within the radius
// Every function must give the same answer when its arguments are swapped (for the two shapes of the same kind).
#include "types.hpp"

namespace col {

inline bool overlaps(const Aabb& a, const Aabb& b) {
    (void)a;
    (void)b;
    return false;  // TODO
}
inline bool overlaps(const Circle& a, const Circle& b) {
    (void)a;
    (void)b;
    return false;  // TODO
}
inline bool overlaps(const Circle& c, const Aabb& box) {
    (void)c;
    (void)box;
    return false;  // TODO
}

}  // namespace col
