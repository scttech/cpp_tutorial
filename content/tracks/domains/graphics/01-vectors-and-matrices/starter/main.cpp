// The vectors and matrices behind every 3D renderer. Read-only. Press Run.
#include <cmath>
#include <cstdio>

#include "mathlib.hpp"

using namespace gfx;

static void print(const char* name, const Mat4& m) {
    std::printf("%s (column-major storage, shown as rows)\n", name);
    for (std::size_t r = 0; r < 4; ++r) std::printf("  %8.3f %8.3f %8.3f %8.3f\n", m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3));
}

int main() {
    const Vec3 a{1, 2, 3}, b{-2, 0.5, 4};
    std::printf("dot %.3f   cross (%.3f, %.3f, %.3f)   length %.6f\n", dot(a, b), cross(a, b).x, cross(a, b).y, cross(a, b).z, length(a));

    const Mat4 model = Mat4::translation({1, 0, 0}) * Mat4::rotation({0, 0, 1}, M_PI / 2).value_or(Mat4::identity());
    print("model = translate(1,0,0) * rotateZ(90 degrees)", model);
    const Vec3 p = model.transform_point({1, 0, 0}).value_or(Vec3{});
    std::printf("the point (1,0,0) moves to (%.3f, %.3f, %.3f)   (rotated first, then translated)\n", p.x, p.y, p.z);

    const auto view = Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    const auto proj = Mat4::perspective(M_PI / 2, 16.0 / 9.0, 1.0, 100.0);
    if (view && proj) {
        const Mat4 mvp = *proj * *view * model;
        const auto clip = mvp.transform_point({1, 0, 0});
        if (clip) std::printf("the same point on screen: ndc (%.3f, %.3f, %.3f)\n", clip->x, clip->y, clip->z);
    }
    const auto inv = model.inverse();
    if (inv) std::printf("model * model^-1 applied to (3,4,5) = (%.3f, %.3f, %.3f)\n", (model * *inv).transform_point({3, 4, 5})->x, (model * *inv).transform_point({3, 4, 5})->y, (model * *inv).transform_point({3, 4, 5})->z);
    std::printf("a vector of length ~1e200: naive %.3g, careful %.3g\n", std::sqrt(1e200 * 1e200 + 1e200 * 1e200), length({1e200, 1e200, 0}));
}
