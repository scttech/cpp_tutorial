// Rotations as quaternions: composing, converting and smoothly interpolating. Read-only. Press Run.
#include <cmath>
#include <cstdio>

#include "quat.hpp"

using namespace gfx;

static void show(const char* name, Vec3 v) { std::printf("%-34s (%7.4f, %7.4f, %7.4f)\n", name, v.x, v.y, v.z); }

int main() {
    const auto turn_z = from_axis_angle({0, 0, 1}, M_PI / 2);    // 90 degrees about z
    const auto turn_x = from_axis_angle({1, 0, 0}, M_PI / 2);
    if (!turn_z || !turn_x) return 1;
    show("x rotated 90 degrees about z", rotate(*turn_z, {1, 0, 0}));
    show("y rotated 90 degrees about x", rotate(*turn_x, {0, 1, 0}));
    const Quat both = *turn_z * *turn_x;   // x first, then z
    show("(0,1,0) by 'x then z'", rotate(both, {0, 1, 0}));
    show("same, by the two steps", rotate(*turn_z, rotate(*turn_x, {0, 1, 0})));

    const auto m = to_matrix(*turn_z);
    std::printf("as a matrix: [%.0f %.0f %.0f; %.0f %.0f %.0f; %.0f %.0f %.0f]\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    const auto back = from_matrix(m);
    if (back) std::printf("and back: w=%.4f x=%.4f y=%.4f z=%.4f\n", back->w, back->x, back->y, back->z);

    const auto from = from_axis_angle({0, 0, 1}, 0.0);
    const auto to = from_axis_angle({0, 0, 1}, 3.0);   // 172 degrees
    std::printf("\nslerp from 0 to 172 degrees about z; the angle covered so far should grow evenly:\n");
    for (int i = 0; i <= 4; ++i) {
        const double t = i / 4.0;
        const Quat q = slerp(*from, *to, t);
        std::printf("  t=%.2f  angle from start %.4f rad   (3.0 * t = %.4f)\n", t, angle_between(*from, q), 3.0 * t);
    }
}
