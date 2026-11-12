#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

#include "raster.hpp"
#include "zth_test.hpp"

using namespace gfx;

namespace {

using i64 = long long;
using Pix = std::set<std::pair<int, int>>;   // (x, y)

// A canvas with guard bytes before and after: drawing outside it is detected.
struct Guarded {
    static constexpr std::size_t kGuard = 4096;
    std::vector<std::uint8_t> mem;
    int w, h;
    Canvas canvas;
    Guarded(int width, int height) : mem(kGuard + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) + kGuard, 0), w(width), h(height), canvas(mem.data() + kGuard, width, height) {
        std::fill(mem.begin(), mem.begin() + static_cast<std::ptrdiff_t>(kGuard), 0xEE);
        std::fill(mem.end() - static_cast<std::ptrdiff_t>(kGuard), mem.end(), 0xEE);
    }
    bool guards_intact() const {
        for (std::size_t i = 0; i < kGuard; ++i)
            if (mem[i] != 0xEE || mem[mem.size() - 1 - i] != 0xEE) return false;
        return true;
    }
    std::uint8_t at(int x, int y) const { return mem[kGuard + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)]; }
    Pix pixels() const {
        Pix p;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (at(x, y)) p.insert({x, y});
        return p;
    }
    std::size_t count() const { return pixels().size(); }
};

// ---- independent references, straight from the definitions -----------------------------------------------------------------------
// the full set of pixels of a line, restricted to the canvas, computed column by column (or row by row) without walking the whole line
Pix ref_line(Point a, Point b, int W, int H) {
    Pix out;
    const i64 dx = static_cast<i64>(b.x) - a.x, dy = static_cast<i64>(b.y) - a.y;
    const bool xmajor = std::llabs(dx) >= std::llabs(dy);
    i64 m0 = xmajor ? a.x : a.y, n0 = xmajor ? a.y : a.x, m1 = xmajor ? b.x : b.y, n1 = xmajor ? b.y : b.x;
    if (m1 < m0) {
        std::swap(m0, m1);
        std::swap(n0, n1);
    }
    const i64 DM = m1 - m0, DN = std::llabs(n1 - n0), sn = n1 >= n0 ? 1 : -1;
    const i64 msize = xmajor ? W : H, nsize = xmajor ? H : W;
    for (i64 m = std::max<i64>(m0, 0); m <= std::min<i64>(m1, msize - 1); ++m) {
        const i64 k = m - m0;
        const i64 n = DM == 0 ? n0 : n0 + sn * ((2 * k * DN + DM) / (2 * DM));
        if (n >= 0 && n < nsize) out.insert(xmajor ? std::make_pair(static_cast<int>(m), static_cast<int>(n)) : std::make_pair(static_cast<int>(n), static_cast<int>(m)));
    }
    return out;
}

// Bresenham's incremental algorithm for the SAME rule (small, fully visible lines): a second implementation to compare with the formula
Pix bresenham(Point a, Point b) {
    Pix out;
    if (std::abs(b.x - a.x) >= std::abs(b.y - a.y)) {
        if (b.x < a.x) std::swap(a, b);
        const int DX = b.x - a.x, DY = std::abs(b.y - a.y), sy = b.y >= a.y ? 1 : -1;
        int y = a.y;
        int num = DX;   // 2 k DY + DX, which starts at DX for k = 0; the minor coordinate has advanced j times when num >= 2 DX j
        int next = 2 * DX;
        for (int k = 0; k <= DX; ++k) {
            if (DX != 0) {
                while (num >= next) {
                    y += sy;
                    next += 2 * DX;
                }
            }
            out.insert({a.x + k, y});
            num += 2 * DY;
        }
    } else {
        Pix t = bresenham({a.y, a.x}, {b.y, b.x});
        for (const auto& p : t) out.insert({p.second, p.first});
    }
    return out;
}

// triangle coverage for one pixel, from the definition
bool ref_covered(Point v0, Point v1, Point v2, int px, int py) {
    const i64 area2 = (static_cast<i64>(v1.x) - v0.x) * (static_cast<i64>(v2.y) - v0.y) - (static_cast<i64>(v1.y) - v0.y) * (static_cast<i64>(v2.x) - v0.x);
    if (area2 == 0) return false;
    if (area2 < 0) std::swap(v1, v2);
    const Point v[3] = {v0, v1, v2};
    const i64 cx = 16LL * px + 8, cy = 16LL * py + 8;
    for (int e = 0; e < 3; ++e) {
        const Point a = v[e], b = v[(e + 1) % 3];
        const i64 w = (static_cast<i64>(b.x) - a.x) * (cy - a.y) - (static_cast<i64>(b.y) - a.y) * (cx - a.x);
        const bool owns = (b.y < a.y) || (b.y == a.y && b.x > a.x);
        if (!(w > 0 || (w == 0 && owns))) return false;
    }
    return true;
}
Pix ref_triangle(Point v0, Point v1, Point v2, int W, int H) {
    Pix p;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (ref_covered(v0, v1, v2, x, y)) p.insert({x, y});
    return p;
}

Point P(int x, int y) { return {x, y}; }
Point fx(int x, int y) { return {16 * x, 16 * y}; }   // whole-pixel positions as 28.4 coordinates (on pixel CORNERS)
Point rp(zth::Lcg& rng, int range) { return {rng.below(2 * range + 1) - range, rng.below(2 * range + 1) - range}; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "lines: horizontal, vertical and diagonal, endpoints included") {
    Guarded g(20, 20);
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(2, 3), P(9, 3), 1));
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(12, 1), P(12, 8), 1));
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(0, 10), P(7, 17), 1));
    Pix want;
    for (int x = 2; x <= 9; ++x) want.insert({x, 3});
    for (int y = 1; y <= 8; ++y) want.insert({12, y});
    for (int i = 0; i <= 7; ++i) want.insert({i, 10 + i});
    ZTH_EXPECT_TRUE(g.pixels() == want);
    ZTH_EXPECT_TRUE(g.guards_intact());
    ZTH_EXPECT_EQ(g.canvas.writes(), want.size());   // nothing drawn twice
}
ZTH_TEST("works", "a single point, and the same line in both directions") {
    Guarded g(10, 10);
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(4, 5), P(4, 5), 7));
    ZTH_EXPECT_TRUE(g.count() == 1 && g.at(4, 5) == 7);
    Guarded a(32, 32), b(32, 32);
    draw_line(a.canvas, P(1, 2), P(30, 11), 1);
    draw_line(b.canvas, P(30, 11), P(1, 2), 1);
    ZTH_EXPECT_TRUE(a.pixels() == b.pixels() && a.count() == 30);
}
ZTH_TEST("works", "the rule for shallow and steep lines: a known pattern, ties rounded half up") {
    Guarded g(12, 6);
    draw_line(g.canvas, P(0, 0), P(10, 4), 1);   // 0.4 per column: y = 0,0,1,1,2,2,2,3,3,4,4 with ties up: floor((8k + 10) / 20)
    const int expect_y[11] = {0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4};
    for (int x = 0; x <= 10; ++x) ZTH_EXPECT_TRUE(g.at(x, expect_y[x]) == 1);
    ZTH_EXPECT_EQ(g.count(), std::size_t{11});
    Guarded s(6, 12);   // the same line seen from the side: steep, x as the minor coordinate
    draw_line(s.canvas, P(0, 0), P(4, 10), 1);
    for (int y = 0; y <= 10; ++y) ZTH_EXPECT_TRUE(s.at(expect_y[y], y) == 1);
    ZTH_EXPECT_EQ(s.count(), std::size_t{11});
    Guarded t(10, 10);   // a perfect tie: slope 1/2, the pixel at k = 1 is y = 1 (0.5 rounds up)
    draw_line(t.canvas, P(0, 0), P(2, 1), 1);
    ZTH_EXPECT_TRUE(t.at(0, 0) && t.at(1, 1) && t.at(2, 1) && t.count() == 3);
    Guarded d(10, 10);   // the downward-left case: sorted first, so the tie goes the same way as for (0,0)-(2,1) seen from its left end
    draw_line(d.canvas, P(2, 0), P(0, 1), 1);
    ZTH_EXPECT_TRUE(d.pixels() == (Pix{{0, 1}, {1, 0}, {2, 0}}));
}
ZTH_TEST("works", "lines in all eight octants have max(|dx|, |dy|) + 1 pixels and match an incremental Bresenham") {
    for (int dx = -9; dx <= 9; dx += 3)
        for (int dy = -9; dy <= 9; dy += 3) {
            Guarded g(30, 30);
            draw_line(g.canvas, P(15, 15), P(15 + dx, 15 + dy), 1);
            ZTH_EXPECT_EQ(g.count(), static_cast<std::size_t>(std::max(std::abs(dx), std::abs(dy)) + 1));
            ZTH_EXPECT_TRUE(g.pixels() == bresenham(P(15, 15), P(15 + dx, 15 + dy)));
            ZTH_EXPECT_TRUE(g.at(15, 15) == 1 && g.at(15 + dx, 15 + dy) == 1);
            ZTH_EXPECT_TRUE(g.guards_intact());
        }
}
ZTH_TEST("works", "lines are clipped to the canvas without changing the pixels that remain") {
    Guarded g(16, 12);
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(-20, -7), P(40, 25), 1));
    ZTH_EXPECT_TRUE(g.guards_intact());
    ZTH_EXPECT_TRUE(g.pixels() == ref_line(P(-20, -7), P(40, 25), 16, 12));
    ZTH_EXPECT_TRUE(g.count() > 8);
    Guarded h(16, 12);
    draw_line(h.canvas, P(-5, 6), P(30, 6), 1);   // along a row, both ends outside
    ZTH_EXPECT_EQ(h.count(), std::size_t{16});
    Guarded v(16, 12);
    draw_line(v.canvas, P(3, -50), P(3, 50), 1);
    ZTH_EXPECT_EQ(v.count(), std::size_t{12});
    Guarded none(16, 12);
    ZTH_EXPECT_TRUE(draw_line(none.canvas, P(-10, -10), P(-1, -1), 1) && none.count() == 0);   // entirely off the canvas, one pixel from it
    ZTH_EXPECT_TRUE(draw_line(none.canvas, P(16, 0), P(40, 5), 1) && none.count() == 0);
    ZTH_EXPECT_TRUE(draw_line(none.canvas, P(0, 12), P(15, 12), 1) && none.count() == 0);
    ZTH_EXPECT_TRUE(none.guards_intact());
}
ZTH_TEST("works", "line coordinates beyond +-2^29 are refused and draw nothing; empty canvases are fine") {
    Guarded g(10, 10);
    ZTH_EXPECT_FALSE(draw_line(g.canvas, P(0, 0), P(kMaxLineCoord + 1, 5), 1));
    ZTH_EXPECT_FALSE(draw_line(g.canvas, P(-kMaxLineCoord - 1, 0), P(3, 5), 1));
    ZTH_EXPECT_FALSE(draw_line(g.canvas, P(0, 0), P(5, 2147483647), 1));
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(0, 0), P(kMaxLineCoord, 5), 1));
    ZTH_EXPECT_TRUE(draw_line(g.canvas, P(-kMaxLineCoord, -kMaxLineCoord), P(kMaxLineCoord, kMaxLineCoord), 1));
    ZTH_EXPECT_TRUE(g.guards_intact());
    std::uint8_t dummy[4] = {0, 0, 0, 0};
    Canvas empty(dummy, 0, 5), flat(dummy, 5, 0);
    ZTH_EXPECT_TRUE(draw_line(empty, P(0, 0), P(3, 3), 1) && draw_line(flat, P(0, 0), P(3, 3), 1));
    ZTH_EXPECT_TRUE(fill_triangle(empty, fx(0, 0), fx(3, 0), fx(0, 3), 1) && fill_triangle(flat, fx(0, 0), fx(3, 0), fx(0, 3), 1));
    ZTH_EXPECT_EQ(empty.writes() + flat.writes(), std::size_t{0});
}
ZTH_TEST("works", "a triangle: pixels whose CENTRES are inside, for a known right triangle") {
    Guarded g(12, 12);
    ZTH_EXPECT_TRUE(fill_triangle(g.canvas, fx(0, 0), fx(8, 0), fx(0, 8), 1));   // corners on pixel corners: the pixel centres at x+0.5 decide
    Pix want;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (x + y + 1 < 8) want.insert({x, y});   // centre (x+.5)+(y+.5) < 8  <=>  x + y < 7
    // centres with x + y == 7 lie exactly ON the hypotenuse; that edge does not own its boundary, so they are left out
    ZTH_EXPECT_TRUE(g.pixels() == want);
    ZTH_EXPECT_EQ(g.count(), std::size_t{28});
    ZTH_EXPECT_TRUE(g.guards_intact());
    ZTH_EXPECT_EQ(g.canvas.writes(), std::size_t{28});
}
ZTH_TEST("works", "any winding gives the same pixels; degenerate triangles draw nothing; the colour is the colour") {
    const Point a = fx(1, 1), b = fx(9, 3), c = fx(4, 9);
    Guarded g1(12, 12), g2(12, 12), g3(12, 12);
    fill_triangle(g1.canvas, a, b, c, 5);
    fill_triangle(g2.canvas, a, c, b, 5);
    fill_triangle(g3.canvas, c, a, b, 5);
    ZTH_EXPECT_TRUE(g1.pixels() == g2.pixels() && g1.pixels() == g3.pixels() && g1.count() > 15);
    ZTH_EXPECT_TRUE(g1.at(4, 4) == 5);
    Guarded d(12, 12);
    ZTH_EXPECT_TRUE(fill_triangle(d.canvas, fx(1, 1), fx(5, 5), fx(9, 9), 1));   // collinear
    ZTH_EXPECT_TRUE(fill_triangle(d.canvas, fx(2, 2), fx(2, 2), fx(8, 3), 1));   // a repeated vertex
    ZTH_EXPECT_TRUE(fill_triangle(d.canvas, fx(3, 3), fx(3, 3), fx(3, 3), 1));
    ZTH_EXPECT_EQ(d.canvas.writes(), std::size_t{0});
}
ZTH_TEST("works", "pixel centres, not corners, decide: a triangle with sub-pixel vertices") {
    Guarded g(8, 8);
    // vertices at (0.5, 0.5), (4.5, 0.5), (0.5, 4.5) in pixel units: centres sit ON the vertices and on the two legs
    fill_triangle(g.canvas, P(8, 8), P(72, 8), P(8, 72), 1);
    // the legs are x = 0.5 (vertical, to the left) and y = 0.5 (horizontal, along the top); the owning edges are the top (a->b with dy == 0, dx > 0) and the left (dy < 0)
    Pix want;
    for (int y = 0; y <= 4; ++y)
        for (int x = 0; x <= 4; ++x)
            if (x + y < 4) want.insert({x, y});   // centres at (x + .5, y + .5): inside or on a leg when x + y <= 3; the hypotenuse x + y = 4 passes through centres with x + y == 4 (not owned)
    ZTH_EXPECT_TRUE(g.pixels() == want);
}
ZTH_TEST("works", "triangle coordinates beyond +-2^28 are refused and draw nothing") {
    Guarded g(10, 10);
    ZTH_EXPECT_FALSE(fill_triangle(g.canvas, P(0, 0), P(kMaxSubpixel + 1, 0), P(0, 50), 1));
    ZTH_EXPECT_FALSE(fill_triangle(g.canvas, P(0, 0), P(10, 10), P(2147483647, 50), 1));
    ZTH_EXPECT_TRUE(fill_triangle(g.canvas, P(0, 0), P(kMaxSubpixel, 0), P(0, kMaxSubpixel), 1));
    ZTH_EXPECT_TRUE(fill_triangle(g.canvas, P(-kMaxSubpixel, -kMaxSubpixel), P(kMaxSubpixel, -kMaxSubpixel), P(0, kMaxSubpixel), 2));
    ZTH_EXPECT_TRUE(g.guards_intact());
    ZTH_EXPECT_EQ(g.canvas.writes(), std::size_t{200});   // both triangles are so large that each covers the whole 10 x 10 canvas
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "1500 random lines (small and enormous) against the formula and, for small ones, an incremental Bresenham") {
    zth::Lcg rng(2024);
    for (int round = 0; round < 1500; ++round) {
        const int W = 1 + rng.below(40), H = 1 + rng.below(40);
        const int range = rng.below(3) == 0 ? kMaxLineCoord : rng.below(2) == 0 ? 60 : 5000;
        Point a = rp(rng, range), b = rng.below(6) == 0 ? Point{a.x + rng.below(7) - 3, a.y + rng.below(7) - 3} : rp(rng, range);
        if (range == kMaxLineCoord) {   // a huge line that really does cross the canvas
            const Point q = {rng.below(W), rng.below(H)}, d = rp(rng, kMaxLineCoord / 2 - 64);
            a = {q.x - d.x, q.y - d.y};
            b = {q.x + d.x, q.y + d.y};
        }
        Guarded g(W, H);
        ZTH_EXPECT_TRUE(draw_line(g.canvas, a, b, 1));
        ZTH_EXPECT_TRUE(g.guards_intact());
        ZTH_EXPECT_TRUE(g.pixels() == ref_line(a, b, W, H));
        ZTH_EXPECT_EQ(g.canvas.writes(), g.count());
        if (range == 60) {
            Pix full = bresenham(a, b), visible;
            for (const auto& p : full)
                if (p.first >= 0 && p.first < W && p.second >= 0 && p.second < H) visible.insert(p);
            ZTH_EXPECT_TRUE(g.pixels() == visible);
        }
        Guarded r(W, H);
        draw_line(r.canvas, b, a, 1);
        ZTH_EXPECT_TRUE(r.pixels() == g.pixels());   // direction does not matter
    }
}
ZTH_TEST("robust", "lines are connected: consecutive pixels of a fully visible line touch, and the line has no gaps or duplicates") {
    zth::Lcg rng(5);
    for (int round = 0; round < 300; ++round) {
        const Point a = rp(rng, 25), b = rp(rng, 25);
        Guarded g(60, 60);
        draw_line(g.canvas, {a.x + 30, a.y + 30}, {b.x + 30, b.y + 30}, 1);
        const int n = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) + 1;
        ZTH_EXPECT_EQ(g.count(), static_cast<std::size_t>(n));
        ZTH_EXPECT_EQ(g.canvas.writes(), static_cast<std::size_t>(n));
        // every pixel has a neighbour (8-connected) in the line except at the ends
        const Pix p = g.pixels();
        int ends = 0;
        for (const auto& q : p) {
            int nb = 0;
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    if ((dx || dy) && p.count({q.first + dx, q.second + dy})) ++nb;
            if (nb < 2) ++ends;
        }
        ZTH_EXPECT_TRUE(ends <= 2 || n <= 2);
    }
}
ZTH_TEST("robust", "300 random triangles (inside, partly outside, enormous) against the coverage definition, pixel by pixel") {
    zth::Lcg rng(77);
    for (int round = 0; round < 300; ++round) {
        const int W = 1 + rng.below(30), H = 1 + rng.below(30);
        const int range = rng.below(4) == 0 ? kMaxSubpixel : rng.below(2) == 0 ? 16 * 40 : 16 * 8;
        const Point v0 = rp(rng, range), v1 = rp(rng, range), v2 = rp(rng, range);
        Guarded g(W, H);
        ZTH_EXPECT_TRUE(fill_triangle(g.canvas, v0, v1, v2, 1));
        ZTH_EXPECT_TRUE(g.guards_intact());
        ZTH_EXPECT_TRUE(g.pixels() == ref_triangle(v0, v1, v2, W, H));
        ZTH_EXPECT_EQ(g.canvas.writes(), g.count());
        Guarded h(W, H);
        fill_triangle(h.canvas, v2, v0, v1, 1);
        ZTH_EXPECT_TRUE(h.pixels() == g.pixels());
    }
}
ZTH_TEST("robust", "triangles whose vertices sit exactly on pixel centres and on pixel corners (lots of ties)") {
    zth::Lcg rng(31);
    for (int round = 0; round < 400; ++round) {
        const int off = rng.below(2) ? 8 : 0;   // centres or corners
        const Point v0 = {16 * rng.below(12) + off, 16 * rng.below(12) + off}, v1 = {16 * rng.below(12) + off, 16 * rng.below(12) + off}, v2 = {16 * rng.below(12) + off, 16 * rng.below(12) + off};
        Guarded g(12, 12);
        fill_triangle(g.canvas, v0, v1, v2, 1);
        ZTH_EXPECT_TRUE(g.pixels() == ref_triangle(v0, v1, v2, 12, 12));
        ZTH_EXPECT_EQ(g.canvas.writes(), g.count());
    }
}
ZTH_TEST("robust", "watertight: a convex quad split along either diagonal, in any winding, covers each pixel exactly once and gives the same pixels") {
    zth::Lcg rng(9);
    for (int round = 0; round < 400; ++round) {
        const int x0 = rng.below(80), y0 = rng.below(80), x1 = x0 + 64 + rng.below(160), y1 = y0 + 64 + rng.below(160);
        const Point p0 = {x0, y0}, p1 = {x1, y0 + rng.below(8)}, p2 = {x1 - rng.below(8), y1}, p3 = {x0 + rng.below(8), y1 - rng.below(8)};
        Guarded a(14, 14), b(14, 14);
        const bool flip = rng.below(2) == 1;
        fill_triangle(a.canvas, p0, flip ? p2 : p1, flip ? p1 : p2, 1);
        fill_triangle(a.canvas, p0, p2, p3, 1);
        fill_triangle(b.canvas, p1, p2, p3, 1);
        fill_triangle(b.canvas, p3, p0, p1, 1);
        ZTH_EXPECT_EQ(a.canvas.writes(), a.count());   // no pixel written twice (that would be a seam)
        ZTH_EXPECT_EQ(b.canvas.writes(), b.count());
        ZTH_EXPECT_TRUE(a.pixels() == b.pixels());     // the diagonal does not matter: no pixel lost, none gained
    }
}
ZTH_TEST("robust", "a fan of triangles around a point tiles a polygon: no overlap, no cracks, and the same pixels as the polygon's rectangle") {
    // the rectangle [2,10] x [3,9] in pixel corners as a fan of four triangles around its centre (6, 6)
    Guarded fan(14, 14), rect(14, 14);
    const Point c = fx(6, 6), a = fx(2, 3), b = fx(10, 3), d = fx(10, 9), e = fx(2, 9);
    fill_triangle(fan.canvas, c, a, b, 1);
    fill_triangle(fan.canvas, c, b, d, 1);
    fill_triangle(fan.canvas, c, d, e, 1);
    fill_triangle(fan.canvas, c, e, a, 1);
    fill_triangle(rect.canvas, a, b, d, 1);
    fill_triangle(rect.canvas, a, d, e, 1);
    ZTH_EXPECT_EQ(fan.canvas.writes(), fan.count());
    ZTH_EXPECT_EQ(fan.count(), std::size_t{8 * 6});
    ZTH_EXPECT_TRUE(fan.pixels() == rect.pixels());
    ZTH_EXPECT_EQ(rect.canvas.writes(), std::size_t{48});
}
ZTH_TEST("robust", "floor division: triangles left of and above the canvas, with vertices at negative coordinates one unit from a pixel centre") {
    for (int shift : {-1, -7, -8, -9, -15, -16, -17, -24, -25, -400}) {
        const Point v0 = {shift, shift}, v1 = {shift + 16 * 6, shift + 3}, v2 = {shift + 5, shift + 16 * 6};
        Guarded g(8, 8);
        fill_triangle(g.canvas, v0, v1, v2, 1);
        ZTH_EXPECT_TRUE(g.pixels() == ref_triangle(v0, v1, v2, 8, 8));
        ZTH_EXPECT_TRUE(g.guards_intact());
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "rasterizing allocates nothing") {
    Guarded g(64, 64);
    zth::mem::Scope scope;
    for (int i = 0; i < 200; ++i) {
        draw_line(g.canvas, P(-1000 + i, 5), P(1000, 60 - i / 4), 1);
        fill_triangle(g.canvas, fx(2, 2 + i % 10), fx(60, 8), fx(20, 61), 2);
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST_TIMEOUT("fast", "200000 enormous lines on a 256 x 256 canvas: the cost follows the canvas, not the length of the line", 20) {
    Guarded g(256, 256);
    zth::Lcg rng(4);
    for (int i = 0; i < 200000; ++i) {
        const Point q = {rng.below(256), rng.below(256)}, d = rp(rng, kMaxLineCoord / 2 - 256);
        draw_line(g.canvas, {q.x - d.x, q.y - d.y}, {q.x + d.x, q.y + d.y}, 1);   // a huge line through a point of the canvas
    }
    ZTH_EXPECT_TRUE(g.guards_intact());
}
ZTH_TEST_TIMEOUT("fast", "huge triangles: only the pixels on the canvas are visited (writes == covered pixels, time follows the canvas)", 20) {
    Guarded g(200, 200);
    zth::Lcg rng(6);
    for (int i = 0; i < 100; ++i) {
        const Point a = rp(rng, kMaxSubpixel), b = rp(rng, kMaxSubpixel), c = rp(rng, kMaxSubpixel);
        fill_triangle(g.canvas, a, b, c, 1);
        fill_triangle(g.canvas, {a.x / 2 + 1600, a.y / 2}, {-b.x / 2, 1600}, {c.x / 2, -c.y / 2 + 1600}, 1);
    }
    for (int i = 0; i < 2000; ++i) fill_triangle(g.canvas, {rng.below(3200), rng.below(3200)}, {rng.below(3200), rng.below(3200)}, {rng.below(3200), rng.below(3200)}, 1);
    ZTH_EXPECT_TRUE(g.guards_intact());
    ZTH_EXPECT_TRUE(g.canvas.writes() > 1000);
}
ZTH_TEST_TIMEOUT("fast", "small triangles on a 4096 x 4096 canvas: only the bounding box is visited, not the whole canvas", 20) {
    Guarded g(4096, 4096);
    zth::Lcg rng(8);
    for (int i = 0; i < 20000; ++i) {
        const int x = 16 * rng.below(4000), y = 16 * rng.below(4000);
        fill_triangle(g.canvas, {x, y}, {x + 16 * 20, y + 40}, {x + 30, y + 16 * 20}, 1);
    }
    ZTH_EXPECT_TRUE(g.canvas.writes() > 20000 * 100);
    ZTH_EXPECT_TRUE(g.guards_intact());
}
ZTH_TEST_TIMEOUT("fast", "full-screen triangles: two of them cover a 512 x 512 canvas 40 times over quickly", 20) {
    Guarded g(512, 512);
    for (int i = 0; i < 40; ++i) {
        fill_triangle(g.canvas, fx(0, 0), fx(512, 0), fx(512, 512), static_cast<std::uint8_t>(1 + i % 200));
        fill_triangle(g.canvas, fx(0, 0), fx(512, 512), fx(0, 512), static_cast<std::uint8_t>(1 + i % 200));
    }
    ZTH_EXPECT_EQ(g.canvas.writes(), std::size_t{40} * 512 * 512);
    ZTH_EXPECT_TRUE(g.guards_intact());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no heap containers") {
    ZTH_EXPECT_NO_RAW_MEMORY("raster.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("raster.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "vector", "new", "function", "virtual"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "integer arithmetic only: no floating point, no math functions") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("raster.hpp"));
    for (const char* word : {"float", "double", "sqrt", "floor", "ceil", "round", "lround", "abs", "fabs", "pow", "atan2", "hypot", "cmath", "__int128"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
