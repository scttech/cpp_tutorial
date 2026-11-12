// Drawing lines and triangles pixel by pixel, in integer arithmetic. Read-only. Press Run.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "raster.hpp"

using namespace gfx;

static void show(const std::vector<std::uint8_t>& px, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) std::putchar(px[static_cast<std::size_t>(y * w + x)] ? '#' : '.');
        std::putchar('\n');
    }
}

int main() {
    const int W = 40, H = 14;
    std::vector<std::uint8_t> px(static_cast<std::size_t>(W * H), 0);
    Canvas canvas(px.data(), W, H);

    draw_line(canvas, {1, 1}, {38, 4}, 1);          // shallow
    draw_line(canvas, {2, 12}, {9, 0}, 1);          // steep
    draw_line(canvas, {-100000, 13}, {100000, 6}, 1);   // mostly off the canvas: must be clipped cheaply
    // triangles take 28.4 fixed-point coordinates: 16 units per pixel, pixel centres at +8
    fill_triangle(canvas, {16 * 20, 16 * 3}, {16 * 36, 16 * 8}, {16 * 22, 16 * 11}, 1);
    show(px, W, H);
    std::printf("writes: %zu\n", canvas.writes());

    // two triangles sharing an edge never overlap and never leave a gap
    std::vector<std::uint8_t> quad(static_cast<std::size_t>(12 * 6), 0);
    Canvas q(quad.data(), 12, 6);
    fill_triangle(q, {16, 16}, {16 * 11, 16 * 2}, {16 * 5, 16 * 5}, 1);
    fill_triangle(q, {16 * 11, 16 * 2}, {16, 16}, {16 * 8, 16 * 6 - 8}, 1);   // opposite winding, shares the edge (1,1)-(11,2)
    std::printf("\nshared edge: %zu pixels written, %s\n", q.writes(), "(each pixel exactly once if writes equals the number of '#')");
    show(quad, 12, 6);
}
