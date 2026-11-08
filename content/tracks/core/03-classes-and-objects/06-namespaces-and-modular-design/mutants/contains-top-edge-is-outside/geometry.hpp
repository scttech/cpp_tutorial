// MUTANT: the top edge counts as outside.
#pragma once

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <vector>

// A tiny geometry library. EVERYTHING in this header lives in namespace geo, so it cannot collide with anybody else's
// `Point`, `Rect` or `area`.
namespace geo {

// Implementation details: not part of the library's interface. By convention they go in a nested `detail` namespace
// (never in the global namespace, and never in an unnamed namespace in a header).
namespace detail {

inline long long end_of(int start, int length) { return static_cast<long long>(start) + length; }

// Do [s1, s1+l1) and [s2, s2+l2) share a position? Intervals are half-open: touching ends do not overlap.
inline bool intervals_overlap(int s1, int l1, int s2, int l2) {
    return l1 > 0 && l2 > 0 && s1 < end_of(s2, l2) && s2 < end_of(s1, l1);
}

}  // namespace detail

struct Point {
    int x = 0;
    int y = 0;

    // A "hidden friend": defined inside the class, but it is a non-member of namespace geo, found by ADL.
    friend bool operator==(const Point&, const Point&) = default;
};

// An axis-aligned rectangle: `origin` is its top-left corner. It covers the points with
// origin.x <= x < origin.x + width and origin.y <= y < origin.y + height (half-open, like iterator ranges).
struct Rect {
    Point origin;
    int width = 0;
    int height = 0;

    friend bool operator==(const Rect&, const Rect&) = default;
};

inline long long area(const Rect& r) { return static_cast<long long>(r.width) * r.height; }

inline bool contains(const Rect& r, const Point& p) {
    return p.x >= r.origin.x && detail::end_of(r.origin.x, r.width) > p.x && p.y > r.origin.y &&
           detail::end_of(r.origin.y, r.height) > p.y;
}

// Do the two rectangles share at least one point? An empty rectangle (zero width or height) overlaps nothing.
inline bool overlaps(const Rect& a, const Rect& b) {
    return detail::intervals_overlap(a.origin.x, a.width, b.origin.x, b.width) &&
           detail::intervals_overlap(a.origin.y, a.height, b.origin.y, b.height);
}

// The smallest rectangle that contains every point. std::invalid_argument if there are no points.
inline Rect bounding_box(const std::vector<Point>& points) {
    if (points.empty()) throw std::invalid_argument("bounding_box: no points");
    int min_x = points[0].x, max_x = points[0].x, min_y = points[0].y, max_y = points[0].y;
    for (const Point& p : points) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    return Rect{Point{min_x, min_y}, max_x - min_x + 1, max_y - min_y + 1};
}

// Output operators live in the SAME namespace as the types they print: that is how the compiler finds them (ADL) from
// anywhere, including from inside std:: templates such as std::ostream_iterator or a generic `show(const T&)`.
inline std::ostream& operator<<(std::ostream& os, const Point& p) { return os << '(' << p.x << ", " << p.y << ')'; }

inline std::ostream& operator<<(std::ostream& os, const Rect& r) {
    return os << '[' << r.origin << ' ' << r.width << 'x' << r.height << ']';
}

}  // namespace geo
