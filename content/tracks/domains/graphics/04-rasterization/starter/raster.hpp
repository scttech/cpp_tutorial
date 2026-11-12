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

inline bool draw_line(Canvas&, Point, Point, std::uint8_t) { return false; }   // TODO
inline bool fill_triangle(Canvas&, Point, Point, Point, std::uint8_t) { return false; }   // TODO

}  // namespace gfx
