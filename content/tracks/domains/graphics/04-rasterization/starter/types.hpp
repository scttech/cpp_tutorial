#pragma once
// Read-only: the canvas the rasterizer draws on.
#include <cstddef>
#include <cstdint>

namespace gfx {

struct Point {
    int x = 0, y = 0;
};

// A width x height array of bytes, row by row. plot() writes WITHOUT checking: drawing outside the canvas corrupts memory,
// so the code that calls it must clip. writes() counts the calls, which the tests use to detect pixels drawn twice.
class Canvas {
public:
    Canvas(std::uint8_t* pixels, int width, int height) : pixels_(pixels), width_(width), height_(height) {}
    int width() const { return width_; }
    int height() const { return height_; }
    void plot(int x, int y, std::uint8_t value) {
        pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = value;
        ++writes_;
    }
    std::uint8_t get(int x, int y) const { return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)]; }
    std::size_t writes() const { return writes_; }

private:
    std::uint8_t* pixels_;
    int width_, height_;
    std::size_t writes_ = 0;
};

}  // namespace gfx
