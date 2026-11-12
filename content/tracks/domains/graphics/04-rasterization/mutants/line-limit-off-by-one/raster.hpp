// MUTANT: the largest allowed coordinate is refused
#pragma once
// YOUR RASTERIZER: integer-only lines and triangles on a Canvas (see types.hpp; Canvas::plot does NOT check bounds).
//
// ---- draw_line(canvas, a, b, color) -> bool ----
// Draws the pixels of the segment from a to b, endpoints included, clipped to the canvas. Returns false and draws nothing if a coordinate is outside
// [-kMaxLineCoord, kMaxLineCoord] (so that the arithmetic below fits in 64 bits); true otherwise, even when nothing is visible.
// THE PIXEL RULE (this is what Bresenham computes incrementally; here you may compute every pixel directly from the formula):
//   * dx = b.x - a.x, dy = b.y - a.y (in 64 bits). If |dx| >= |dy| the line is x-major, otherwise y-major (swap the roles of x and y everywhere below).
//   * Sort the two endpoints along the major axis (for x-major: so that the first has the smaller x). This makes draw_line(a, b) and draw_line(b, a) identical.
//   * Let (x0, y0) be the first endpoint, DX = |dx|, DY = |dy|, sy = +1 if the minor coordinate increases along the line, else -1.
//   * For every k = 0 .. DX the pixel is   x = x0 + k,   y = y0 + sy * ((2 * k * DY + DX) / (2 * DX))     (integer division; the minor coordinate is rounded HALF UP).
//     If DX = 0 (a single point) just the one pixel.
// CLIPPING must not change which pixels are drawn: the visible pixels are exactly the pixels of the unclipped line that fall on the canvas. A line from (-500000000, 3) to
// (500000000, 9) on a 100 x 100 canvas must take time proportional to the canvas, not to the length of the line: visit only the k whose major coordinate is on the canvas.
//
// ---- fill_triangle(canvas, v0, v1, v2, color) -> bool ----
// Vertices are in 28.4 FIXED POINT: 16 units per pixel, so the centre of pixel (x, y) is the point (16 x + 8, 16 y + 8). Returns false and draws nothing if a coordinate is outside
// [-kMaxSubpixel, kMaxSubpixel]; true otherwise.
//   * area2 = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x). If area2 == 0 the triangle is degenerate: draw nothing. If area2 < 0 swap v1 and v2 (any winding works).
//   * For each edge a -> b taken in the order v0 -> v1, v1 -> v2, v2 -> v0 and each point p:  w = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x).
//     Inside the triangle all three w are positive.
//   * A pixel is covered when its centre has w > 0 for all three edges, or w == 0 on an edge that OWNS its boundary and w > 0 on the others.
//     An edge a -> b owns its boundary if  (b.y - a.y) < 0  or  (b.y == a.y and b.x - a.x > 0)   (the "top-left rule"): of two triangles that share an edge one traverses it
//     as a -> b and the other as b -> a, and exactly one of the two directions owns it, so a pixel centre exactly on a shared edge is drawn once, never twice and never zero times.
//   * Visit only the pixels inside the bounding box of the triangle AND the canvas. Pixel centres are at 16 x + 8: when you convert a coordinate to a pixel index remember that C++
//     division truncates (-1 / 16 is 0, the pixel containing -1 is -1); a floor division is the right tool.
//   * 64-bit arithmetic only (the products fit); no floating point anywhere.
#include <algorithm>
#include <cstdint>

#include "types.hpp"

namespace gfx {

inline constexpr int kMaxLineCoord = 1 << 29;
inline constexpr int kMaxSubpixel = 1 << 28;

namespace detail {

using i64 = std::int64_t;

// floor(a / b) for b > 0, also for negative a
inline i64 floor_div(i64 a, i64 b) {
    i64 q = a / b;
    if ((a % b != 0) && (a < 0)) --q;
    return q;
}

inline bool in_range(Point p, int limit) { return p.x > -limit && p.x < limit && p.y > -limit && p.y < limit; }

// draws the pixels of a line whose major axis is the first coordinate; `put(major, minor)` is called for the visible ones
template <class Put>
inline void walk(i64 x0, i64 y0, i64 x1, i64 y1, i64 major_size, i64 minor_size, Put put) {
    // sort along the major axis
    if (x1 < x0) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    const i64 DX = x1 - x0;
    const i64 DY = y1 >= y0 ? y1 - y0 : y0 - y1;
    const i64 sy = y1 >= y0 ? 1 : -1;
    // only the k whose major coordinate is on the canvas
    const i64 k_lo = std::max<i64>(0, -x0), k_hi = std::min<i64>(DX, major_size - 1 - x0);
    for (i64 k = k_lo; k <= k_hi; ++k) {
        const i64 y = DX == 0 ? y0 : y0 + sy * ((2 * k * DY + DX) / (2 * DX));
        if (y >= 0 && y < minor_size) put(x0 + k, y);
    }
}

}  // namespace detail

inline bool draw_line(Canvas& c, Point a, Point b, std::uint8_t color) {
    using detail::i64;
    if (!detail::in_range(a, kMaxLineCoord) || !detail::in_range(b, kMaxLineCoord)) return false;
    if (c.width() <= 0 || c.height() <= 0) return true;
    const i64 dx = static_cast<i64>(b.x) - a.x, dy = static_cast<i64>(b.y) - a.y;
    const i64 adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (adx >= ady) detail::walk(a.x, a.y, b.x, b.y, c.width(), c.height(), [&](i64 x, i64 y) { c.plot(static_cast<int>(x), static_cast<int>(y), color); });
    else detail::walk(a.y, a.x, b.y, b.x, c.height(), c.width(), [&](i64 y, i64 x) { c.plot(static_cast<int>(x), static_cast<int>(y), color); });
    return true;
}

inline bool fill_triangle(Canvas& c, Point v0, Point v1, Point v2, std::uint8_t color) {
    using detail::i64;
    if (!detail::in_range(v0, kMaxSubpixel) || !detail::in_range(v1, kMaxSubpixel) || !detail::in_range(v2, kMaxSubpixel)) return false;
    if (c.width() <= 0 || c.height() <= 0) return true;
    const i64 area2 = (static_cast<i64>(v1.x) - v0.x) * (static_cast<i64>(v2.y) - v0.y) - (static_cast<i64>(v1.y) - v0.y) * (static_cast<i64>(v2.x) - v0.x);
    if (area2 == 0) return true;
    if (area2 < 0) std::swap(v1, v2);
    const Point v[3] = {v0, v1, v2};
    // the pixels whose centres (16 x + 8) lie inside the bounding box, clipped to the canvas
    const i64 min_x = std::min({v[0].x, v[1].x, v[2].x}), max_x = std::max({v[0].x, v[1].x, v[2].x});
    const i64 min_y = std::min({v[0].y, v[1].y, v[2].y}), max_y = std::max({v[0].y, v[1].y, v[2].y});
    const i64 px_lo = std::max<i64>(0, -detail::floor_div(-(min_x - 8), 16));   // ceil((min_x - 8) / 16)
    const i64 px_hi = std::min<i64>(c.width() - 1, detail::floor_div(max_x - 8, 16));
    const i64 py_lo = std::max<i64>(0, -detail::floor_div(-(min_y - 8), 16));
    const i64 py_hi = std::min<i64>(c.height() - 1, detail::floor_div(max_y - 8, 16));
    bool owns[3];
    for (int e = 0; e < 3; ++e) {
        const Point a = v[e], b = v[(e + 1) % 3];
        owns[e] = (b.y < a.y) || (b.y == a.y && b.x > a.x);
    }
    for (i64 py = py_lo; py <= py_hi; ++py)
        for (i64 px = px_lo; px <= px_hi; ++px) {
            const i64 cx = 16 * px + 8, cy = 16 * py + 8;
            bool inside = true;
            for (int e = 0; e < 3 && inside; ++e) {
                const Point a = v[e], b = v[(e + 1) % 3];
                const i64 w = (static_cast<i64>(b.x) - a.x) * (cy - a.y) - (static_cast<i64>(b.y) - a.y) * (cx - a.x);
                inside = w > 0 || (w == 0 && owns[e]);
            }
            if (inside) c.plot(static_cast<int>(px), static_cast<int>(py), color);
        }
    return true;
}

}  // namespace gfx
