// Shooting rays at spheres, planes, boxes and triangles. Read-only. Press Run.
#include <cstdio>
#include <optional>

#include "ray.hpp"

using namespace gfx;

static void report(const char* what, const std::optional<Hit>& h) {
    if (!h) {
        std::printf("%-34s miss\n", what);
        return;
    }
    std::printf("%-34s t=%-7.3f point (%6.2f, %6.2f, %6.2f)  normal (%5.2f, %5.2f, %5.2f)  %s\n", what, h->t, h->point.x, h->point.y, h->point.z, h->normal.x, h->normal.y, h->normal.z,
                h->front ? "front face" : "back face");
}

int main() {
    const Ray down{{0, 0, 10}, {0, 0, -1}};
    report("sphere r=2 at the origin", intersect(down, Sphere{{0, 0, 0}, 2}));
    report("sphere, ray starting inside", intersect(Ray{{0, 0, 0}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2}));
    report("sphere, ray pointing away", intersect(Ray{{0, 0, 10}, {0, 0, 1}}, Sphere{{0, 0, 0}, 2}));
    report("sphere, only hits beyond t_max", intersect(down, Sphere{{0, 0, 0}, 2}, 0, 5));
    report("plane z = 1 (tilted normal)", intersect(down, Plane{{0, 0, 1}, {0, 0.6, 0.8}}));
    report("box [-1,1]^3, from above", intersect(down, Aabb{{-1, -1, -1}, {1, 1, 1}}));
    report("box, axis-parallel ray on a face", intersect(Ray{{1, 0, 10}, {0, 0, -1}}, Aabb{{-1, -1, -1}, {1, 1, 1}}));
    report("triangle", intersect(down, Triangle{{-1, -1, 0}, {3, -1, 0}, {-1, 3, 0}}));
    report("same triangle, seen from below", intersect(Ray{{0, 0, -10}, {0, 0, 1}}, Triangle{{-1, -1, 0}, {3, -1, 0}, {-1, 3, 0}}));
    report("triangle edge-on (parallel)", intersect(Ray{{-5, 0, 0}, {1, 0, 0}}, Triangle{{-1, -1, 0}, {3, -1, 0}, {-1, 3, 0}}));
    // 100 million units away: the textbook discriminant has no digits left
    report("sphere r=1 at distance 1e8", intersect(Ray{{0, 0, 0}, {0, 0, 1}}, Sphere{{0.5, 0, 1e8}, 1}));
}
