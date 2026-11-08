#pragma once

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <vector>

// TODO: never write `using namespace std;` in a header: it forces std's names on EVERY file that includes this one.
// Delete it and write `std::` where needed.
using namespace std;

// TODO: this helper is an implementation detail, but it sits in the global namespace where anyone can collide with
// it. Move it into `namespace geo { namespace detail { ... } }` (nested, and not unnamed: that is for .cpp files).
inline bool intervals_overlap(int s1, int l1, int s2, int l2) {
    return l1 > 0 && l2 > 0 && (long long)s1 < (long long)s2 + l2 && (long long)s2 < (long long)s1 + l1;
}

namespace geo {

struct Point {
    int x = 0;
    int y = 0;
    // TODO: `==` for Point: a defaulted `friend bool operator==(const Point&, const Point&) = default;` right here.
};

// The rectangle covers origin.x <= x < origin.x + width and origin.y <= y < origin.y + height (half-open).
struct Rect {
    Point origin;
    int width = 0;
    int height = 0;
    // TODO: `==` for Rect (the same way).
};

inline long long area(const Rect& r) { return static_cast<long long>(r.width) * r.height; }

inline bool contains(const Rect& r, const Point& p) {
    return p.x >= r.origin.x && (long long)p.x < (long long)r.origin.x + r.width && p.y >= r.origin.y &&
           (long long)p.y < (long long)r.origin.y + r.height;
}

inline bool overlaps(const Rect& a, const Rect& b) {
    return intervals_overlap(a.origin.x, a.width, b.origin.x, b.width) &&
           intervals_overlap(a.origin.y, a.height, b.origin.y, b.height);
}

inline Rect bounding_box(const vector<Point>& points) {
    if (points.empty()) throw invalid_argument("bounding_box: no points");
    int min_x = points[0].x, max_x = points[0].x, min_y = points[0].y, max_y = points[0].y;
    for (const Point& p : points) {
        min_x = min(min_x, p.x);
        max_x = max(max_x, p.x);
        min_y = min(min_y, p.y);
        max_y = max(max_y, p.y);
    }
    return Rect{Point{min_x, min_y}, max_x - min_x + 1, max_y - min_y + 1};
}

}  // namespace geo

// TODO: these four belong INSIDE namespace geo, next to the types. Outside it, they only work when the code that uses
// them can see the global namespace: not from templates in other namespaces (the compiler looks for an operator in the
// namespaces of the ARGUMENTS: argument-dependent lookup). Move them (and change `geo::Point` to `Point`).
inline bool operator==(const geo::Point& a, const geo::Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator==(const geo::Rect& a, const geo::Rect& b) {
    return a.origin == b.origin && a.width == b.width && a.height == b.height;
}
inline std::ostream& operator<<(std::ostream& os, const geo::Point& p) { return os << '(' << p.x << ", " << p.y << ')'; }
inline std::ostream& operator<<(std::ostream& os, const geo::Rect& r) {
    return os << '[' << r.origin << ' ' << r.width << 'x' << r.height << ']';
}
