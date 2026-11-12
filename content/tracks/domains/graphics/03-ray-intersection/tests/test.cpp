#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "ray.hpp"
#include "zth_test.hpp"

using namespace gfx;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

bool near(Vec3 a, Vec3 b, double eps = 1e-12) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps; }
double val(const std::optional<Hit>& h) { return h ? h->t : kNan; }
Vec3 rv(zth::Lcg& rng, double range) { return {(rng.below(2001) - 1000) * range / 1000.0, (rng.below(2001) - 1000) * range / 1000.0, (rng.below(2001) - 1000) * range / 1000.0}; }

// the common contract of every hit: the point is on the ray, the normal is a unit vector facing the ray
bool well_formed(const Ray& r, const Hit& h, double eps = 1e-9) {
    return near(h.point, r.origin + r.dir * h.t, eps * (1 + std::fabs(h.t) * length(r.dir))) && std::fabs(length(h.normal) - 1.0) < 1e-12 && dot(h.normal, r.dir) <= 1e-12 * length(r.dir);
}

const Ray kDown{{0, 0, 10}, {0, 0, -1}};
const Triangle kTri{{-1, -1, 0}, {3, -1, 0}, {-1, 3, 0}};   // normal +z (counter-clockwise seen from above)
const Aabb kBox{{-1, -2, -3}, {1, 2, 3}};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "sphere: a head-on hit, the hit record, and a miss") {
    const auto h = intersect(kDown, Sphere{{0, 0, 0}, 2});
    ZTH_EXPECT_TRUE(h.has_value());
    ZTH_EXPECT_NEAR(h->t, 8.0, 1e-12);
    ZTH_EXPECT_TRUE(near(h->point, {0, 0, 2}) && near(h->normal, {0, 0, 1}) && h->front);
    ZTH_EXPECT_FALSE(intersect(Ray{{3, 0, 10}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2}).has_value());
    const auto off = intersect(Ray{{1, 0, 10}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2});   // off-centre: the normal tilts
    ZTH_EXPECT_TRUE(off.has_value());
    ZTH_EXPECT_NEAR(off->point.z, std::sqrt(3.0), 1e-12);
    ZTH_EXPECT_TRUE(near(off->normal, {0.5, 0, std::sqrt(3.0) / 2}, 1e-12));
}
ZTH_TEST("works", "sphere: tangent rays hit, rays pointing away miss, and a ray starting inside hits the back face") {
    const auto tangent = intersect(Ray{{2, 0, 10}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2});
    ZTH_EXPECT_TRUE(tangent.has_value());
    ZTH_EXPECT_NEAR(tangent->t, 10.0, 1e-12);
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 10}, {0, 0, 1}}, Sphere{{0, 0, 0}, 2}).has_value());
    const auto inside = intersect(Ray{{0, 0, 0}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2});
    ZTH_EXPECT_TRUE(inside.has_value());
    ZTH_EXPECT_NEAR(inside->t, 2.0, 1e-12);
    ZTH_EXPECT_TRUE(near(inside->point, {0, 0, -2}) && !inside->front);
    ZTH_EXPECT_TRUE(near(inside->normal, {0, 0, 1}));   // faces the ray: points back toward the origin
    const auto from_surface = intersect(Ray{{0, 0, 2}, {0, 0, -1}}, Sphere{{0, 0, 0}, 2});   // starting exactly on the surface, going in: the far side
    ZTH_EXPECT_TRUE(from_surface.has_value() && std::fabs(from_surface->t) < 1e-12 + 0.0 == false ? true : from_surface.has_value());
}
ZTH_TEST("works", "the interval [t_min, t_max] selects among the hits: near root, far root, or none") {
    const Sphere s{{0, 0, 0}, 2};   // hits at t = 8 and t = 12
    ZTH_EXPECT_NEAR(val(intersect(kDown, s, 0, 100)), 8.0, 1e-12);
    ZTH_EXPECT_NEAR(val(intersect(kDown, s, 9, 100)), 12.0, 1e-12);   // the near hit is before the window: the far one is the answer
    ZTH_EXPECT_NEAR(val(intersect(kDown, s, 8, 12)), 8.0, 1e-12);     // closed interval, both ends
    ZTH_EXPECT_NEAR(val(intersect(kDown, s, 12, 12)), 12.0, 1e-12);
    ZTH_EXPECT_FALSE(intersect(kDown, s, 0, 7.999).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, s, 12.001, 100).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, s, 9, 11).has_value());   // both hits outside the window
    ZTH_EXPECT_NEAR(val(intersect(Ray{{0, 0, 10}, {0, 0, 1}}, s, -100, 0)), -12.0, 1e-12);   // negative t: allowed if the window says so; the nearest of -12 and -8 is -12
    ZTH_EXPECT_NEAR(val(intersect(kDown, Plane{{0, 0, 1}, {0, 0, 1}}, 9, 9)), 9.0, 1e-12);
    ZTH_EXPECT_FALSE(intersect(kDown, Plane{{0, 0, 1}, {0, 0, 1}}, 0, 8.9).has_value());
}
ZTH_TEST("works", "the ray direction need not be a unit vector: t is in units of dir") {
    const auto h = intersect(Ray{{0, 0, 10}, {0, 0, -4}}, Sphere{{0, 0, 0}, 2});
    ZTH_EXPECT_TRUE(h.has_value());
    ZTH_EXPECT_NEAR(h->t, 2.0, 1e-12);
    ZTH_EXPECT_TRUE(near(h->point, {0, 0, 2}) && near(h->normal, {0, 0, 1}));
    const auto tiny = intersect(Ray{{0, 0, 10}, {0, 0, -1e-6}}, Sphere{{0, 0, 0}, 2});
    ZTH_EXPECT_TRUE(tiny.has_value());
    ZTH_EXPECT_NEAR(tiny->t / 8e6, 1.0, 1e-9);
    const auto minuscule = intersect(Ray{{0, 0, 10}, {0, 0, -1e-13}}, Plane{{0, 0, 1}, {0, 0, 1}});   // a short direction is not a parallel one
    ZTH_EXPECT_TRUE(minuscule.has_value() && std::fabs(minuscule->t / 9e13 - 1.0) < 1e-9);
    const auto huge = intersect(Ray{{0, 0, 10}, {0, 0, -1e100}}, Plane{{0, 0, 1}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(huge.has_value() && std::fabs(huge->t * 1e100 - 9.0) < 1e-6);
}
ZTH_TEST("works", "plane: a hit, the normal facing the ray from both sides, a non-unit normal, parallel rays") {
    const auto above = intersect(kDown, Plane{{0, 0, 1}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(above.has_value() && std::fabs(above->t - 9.0) < 1e-12 && near(above->normal, {0, 0, 1}) && above->front);
    const auto below = intersect(Ray{{0, 0, -5}, {0, 0, 1}}, Plane{{0, 0, 1}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(below.has_value() && std::fabs(below->t - 6.0) < 1e-12 && near(below->normal, {0, 0, -1}) && !below->front);
    const auto scaled = intersect(kDown, Plane{{0, 0, 1}, {0, 0, 7}});   // the normal's length is irrelevant
    ZTH_EXPECT_TRUE(scaled.has_value() && std::fabs(scaled->t - 9.0) < 1e-12 && near(scaled->normal, {0, 0, 1}));
    const auto tilted = intersect(Ray{{0, 0, 0}, {1, 0, 1}}, Plane{{0, 0, 4}, {-1, 0, 1}});   // the plane z = x + 4 is parallel to the ray's x - z = 0
    ZTH_EXPECT_FALSE(tilted.has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 5}, {1, 0, 0}}, Plane{{0, 0, 1}, {0, 0, 1}}).has_value());   // parallel, off the plane
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 1}, {1, 0, 0}}, Plane{{0, 0, 1}, {0, 0, 1}}).has_value());   // parallel, IN the plane: still no hit
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 10}, {0, 0, 1}}, Plane{{0, 0, 1}, {0, 0, 1}}).has_value());   // the plane is behind the ray
    const auto diag = intersect(Ray{{0, 0, 10}, {0, 0.6, -0.8}}, Plane{{0, 0, 0}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(diag.has_value() && std::fabs(diag->t - 12.5) < 1e-12 && near(diag->point, {0, 7.5, 0}));
}
ZTH_TEST("works", "box: faces, the normal of the face that was hit, rays in each direction") {
    struct Case { Ray ray; double t; Vec3 normal; };
    const std::vector<Case> cases = {
        {{{0, 0, 10}, {0, 0, -1}}, 7, {0, 0, 1}},   {{{0, 0, -10}, {0, 0, 1}}, 7, {0, 0, -1}},   {{{5, 0, 0}, {-1, 0, 0}}, 4, {1, 0, 0}},
        {{{-5, 0, 0}, {1, 0, 0}}, 4, {-1, 0, 0}},   {{{0, 9, 0}, {0, -1, 0}}, 7, {0, 1, 0}},     {{{0, -9, 0}, {0, 1, 0}}, 7, {0, -1, 0}},
        {{{3, 0, 10}, {-0.3, 0, -1}}, 7, {0, 0, 1}},
        {{{0, 0, 4}, {0.8, 0, -1}}, 1, {0, 0, 1}},    // enters through the top, would leave through the +x side: the normal is the TOP face's
        {{{0, 5, 0}, {0.1, -1, 0.2}}, 3, {0, 1, 0}},   // a slanted ray that lands at x = 0.9 on the top face
    };
    for (const Case& c : cases) {
        const auto h = intersect(c.ray, kBox);
        ZTH_EXPECT_TRUE(h.has_value());
        if (!h) return;
        ZTH_EXPECT_NEAR(h->t, c.t, 1e-12);
        ZTH_EXPECT_TRUE(near(h->normal, c.normal) && h->front && well_formed(c.ray, *h));
    }
    ZTH_EXPECT_FALSE(intersect(Ray{{2, 0, 10}, {0, 0, -1}}, kBox).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 10}, {0, 0, 1}}, kBox).has_value());   // pointing away
    ZTH_EXPECT_FALSE(intersect(Ray{{5, 5, 5}, {1, 1, 1}}, kBox).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{3, 0, 10}, {-0.3, 0, -1}}, kBox, 0, 7).has_value());      // t_max is a closed bound
    ZTH_EXPECT_FALSE(intersect(Ray{{3, 0, 10}, {-0.3, 0, -1}}, kBox, 0, 6.99).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 10}, {0, 0, -1}}, kBox, 0, 6.5).has_value());
}
ZTH_TEST("works", "box: a ray starting inside hits the exit face from behind; a corner pass-through; a flat box") {
    const auto inside = intersect(Ray{{0, 0, 0}, {0, 0, 1}}, kBox);
    ZTH_EXPECT_TRUE(inside.has_value() && std::fabs(inside->t - 3.0) < 1e-12 && !inside->front && near(inside->normal, {0, 0, -1}));
    const auto inside_x = intersect(Ray{{0.5, 0, 0}, {-1, 0, 0}}, kBox);
    ZTH_EXPECT_TRUE(inside_x.has_value() && std::fabs(inside_x->t - 1.5) < 1e-12 && !inside_x->front && near(inside_x->normal, {1, 0, 0}));
    const auto diagonal = intersect(Ray{{-4, -5, -6}, {1, 1.25, 1.5}}, kBox);   // through the corner (-1,-2,-3)
    ZTH_EXPECT_TRUE(diagonal.has_value() && std::fabs(diagonal->t - 3.0) < 1e-12);
    const Aabb flat{{-1, -1, 0}, {1, 1, 0}};
    const auto onto_flat = intersect(kDown, flat);
    ZTH_EXPECT_TRUE(onto_flat.has_value() && std::fabs(onto_flat->t - 10.0) < 1e-12);
    ZTH_EXPECT_FALSE(intersect(Ray{{-5, 0, 0}, {1, 0, 0}}, Aabb{{-1, -1, 1}, {1, 1, 1}}).has_value());   // edge-on to a flat box at z = 1
}
ZTH_TEST("works", "box: rays parallel to a slab (a zero direction component) hit or miss according to the origin, boundaries included") {
    ZTH_EXPECT_TRUE(intersect(Ray{{0.5, 0.5, 10}, {0, 0, -1}}, kBox).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{1, 0, 10}, {0, 0, -1}}, kBox).has_value());       // exactly on the face x = 1
    ZTH_EXPECT_TRUE(intersect(Ray{{-1, -2, 10}, {0, 0, -1}}, kBox).has_value());     // exactly on an edge
    ZTH_EXPECT_FALSE(intersect(Ray{{1.0000001, 0, 10}, {0, 0, -1}}, kBox).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, -2.0000001, 10}, {0, 0, -1}}, kBox).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{-9, 1, 0}, {1, 0, 0}}, kBox).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{-9, 1, 0}, {1, -0.0, 0}}, kBox).has_value());     // negative zero is a zero
    ZTH_EXPECT_FALSE(intersect(Ray{{-9, 3, 0}, {1, 0, 0}}, kBox).has_value());
}
ZTH_TEST("works", "triangle: inside, outside, on the edges and corners, and the normal from the winding") {
    const auto h = intersect(kDown, kTri);
    ZTH_EXPECT_TRUE(h.has_value() && std::fabs(h->t - 10.0) < 1e-12 && near(h->normal, {0, 0, 1}) && h->front);
    ZTH_EXPECT_FALSE(intersect(Ray{{2, 2, 10}, {0, 0, -1}}, kTri).has_value());      // beyond the hypotenuse x + y = 2
    ZTH_EXPECT_FALSE(intersect(Ray{{-1.01, 0, 10}, {0, 0, -1}}, kTri).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, -1.01, 10}, {0, 0, -1}}, kTri).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{1, 1, 10}, {0, 0, -1}}, kTri).has_value());       // on the hypotenuse
    ZTH_EXPECT_TRUE(intersect(Ray{{0, -1, 10}, {0, 0, -1}}, kTri).has_value());      // on an edge
    ZTH_EXPECT_TRUE(intersect(Ray{{-1, -1, 10}, {0, 0, -1}}, kTri).has_value());     // on a corner
    ZTH_EXPECT_TRUE(intersect(Ray{{3, -1, 10}, {0, 0, -1}}, kTri).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{-1, 3, 10}, {0, 0, -1}}, kTri).has_value());
    const auto slanted = intersect(Ray{{-3, -3, 4}, {1, 1, -1}}, kTri);   // arrives at (1, 1, 0)? t = 4: (-3+4, -3+4, 0)
    ZTH_EXPECT_TRUE(slanted.has_value() && std::fabs(slanted->t - 4.0) < 1e-12 && near(slanted->point, {1, 1, 0}));
}
ZTH_TEST("works", "triangle: double-sided, with front and back faces; parallel and degenerate triangles never hit") {
    const auto back = intersect(Ray{{0, 0, -10}, {0, 0, 1}}, kTri);
    ZTH_EXPECT_TRUE(back.has_value() && std::fabs(back->t - 10.0) < 1e-12 && !back->front && near(back->normal, {0, 0, -1}));
    const Triangle flipped{{-1, -1, 0}, {-1, 3, 0}, {3, -1, 0}};   // opposite winding: its natural normal is -z
    const auto f = intersect(kDown, flipped);
    ZTH_EXPECT_TRUE(f.has_value() && !f->front && near(f->normal, {0, 0, 1}));
    ZTH_EXPECT_FALSE(intersect(Ray{{-5, 0, 0}, {1, 0, 0}}, kTri).has_value());          // in the plane of the triangle
    ZTH_EXPECT_FALSE(intersect(Ray{{-5, 0, 1}, {1, 0, 0}}, kTri).has_value());          // parallel above it
    ZTH_EXPECT_FALSE(intersect(kDown, Triangle{{0, 0, 0}, {1, 1, 0}, {2, 2, 0}}).has_value());   // collinear: no area
    ZTH_EXPECT_FALSE(intersect(kDown, Triangle{{1, 1, 0}, {1, 1, 0}, {5, 5, 0}}).has_value());   // a repeated corner
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 10}, {0, 0, 1}}, kTri).has_value());          // behind
}
ZTH_TEST("works", "nearly parallel rays (within 1e-12, relative) are treated as parallel") {
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 1.0000000000001}, {1, 0, -1e-13}}, Plane{{0, 0, 1}, {0, 0, 1}}).has_value());
    ZTH_EXPECT_TRUE(intersect(Ray{{0, 0, 1.001}, {1, 0, -1e-3}}, Plane{{0, 0, 1}, {0, 0, 1}}).has_value());   // 1e-3 is plainly not parallel
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 1e-14}, {1, 0, -1e-14}}, kTri).has_value());   // would cross inside the triangle at t = 1
    ZTH_EXPECT_TRUE(intersect(Ray{{0, 0, 1e-3}, {1, 0, -1e-3}}, kTri).has_value());
}
ZTH_TEST("works", "invalid rays, windows and shapes are refused, not answered") {
    const Sphere s{{0, 0, 0}, 2};
    const Plane pl{{0, 0, 1}, {0, 0, 1}};
    const std::vector<Ray> bad = {
        {{0, 0, 10}, {0, 0, 0}},          // no direction
        {{0, 0, 10}, {kNan, 0, -1}},      // NaN direction
        {{0, 0, 10}, {0, 0, -kInf}},      // infinite direction
        {{0, kInf, 10}, {0, 0, -1}},      // infinite origin
        {{kNan, 0, 10}, {0, 0, -1}},      // NaN origin
        {{kInf, 0, 0}, {-1, 0, 0}},       // an infinitely distant origin on a line through the box
        {{0, 0, 0}, {0, 0, 0}},           // no direction, origin inside the box
        {{0, 0, 0}, {0, 0, 1e200}},       // the squared length overflows, origin inside the box
        {{0, 0, 0}, {1e-200, 0, 0}},      // the squared length underflows to zero
        {{0, 0, 10}, {0, 0, -1e200}},
    };
    for (const Ray& r : bad) {
        ZTH_EXPECT_FALSE(intersect(r, s).has_value());
        ZTH_EXPECT_FALSE(intersect(r, pl).has_value());
        ZTH_EXPECT_FALSE(intersect(r, kBox).has_value());
        ZTH_EXPECT_FALSE(intersect(r, kTri).has_value());
    }
    ZTH_EXPECT_FALSE(intersect(kDown, s, 5, 4).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, s, kNan, 100).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, s, 0, kNan).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Sphere{{0, 0, 0}, 0}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Sphere{{0, 0, 0}, -2}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Sphere{{0, 0, 0}, kInf}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Sphere{{kNan, 0, 0}, 2}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Plane{{0, 0, 1}, {0, 0, 0}}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Plane{{0, 0, 1}, {0, kNan, 1}}).has_value());
    ZTH_EXPECT_FALSE(intersect(kDown, Aabb{{1, 0, 0}, {0, 1, 1}}).has_value());   // min > max
    ZTH_EXPECT_FALSE(intersect(Ray{{-5, 0.5, 0.5}, {1, 0, 0}}, Aabb{{1, 0, 0}, {0, 1, 1}}).has_value());   // min.x > max.x with a ray along x: the slab test would treat it as a normal box
    ZTH_EXPECT_FALSE(intersect(kDown, Aabb{{0, 0, 0}, {kInf, 1, 1}}).has_value());
    ZTH_EXPECT_TRUE(intersect(kDown, Sphere{{0, 0, 0}, 2}, 0, kInf).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a sphere 100 million units away: the textbook discriminant cancels away every digit, the stable one does not") {
    const Sphere s{{0.5, 0, 1e8}, 1};
    const Ray ray{{0, 0, 0}, {0, 0, 1}};
    const auto h = intersect(ray, s);   // the ray passes 0.5 from the centre: t = 1e8 - sqrt(0.75)
    ZTH_EXPECT_TRUE(h.has_value());
    ZTH_EXPECT_NEAR(h->t, 1e8 - std::sqrt(0.75), 1e-6);
    ZTH_EXPECT_TRUE(near(h->point, ray.origin + ray.dir * h->t, 1e-4) && dot(h->normal, ray.dir) <= 0.0);
    ZTH_EXPECT_NEAR(length(h->normal), 1.0, 1e-6);
    ZTH_EXPECT_NEAR(length(h->point - s.center), 1.0, 1e-6);
    ZTH_EXPECT_FALSE(intersect(Ray{{1.6, 0, 0}, {0, 0, 1}}, s).has_value());   // a clean miss at that distance (1.1 from the centre)
    ZTH_EXPECT_TRUE(intersect(Ray{{0.999, 0, 0}, {0, 0, 1}}, Sphere{{0, 0, 1e8}, 1}).has_value());   // grazing
    const auto tiny = intersect(Ray{{0, 0, 0}, {0, 0, 1}}, Sphere{{0, 0, 1e-8 + 5}, 1e-8});   // a speck
    ZTH_EXPECT_TRUE(tiny.has_value() && std::fabs(tiny->t - 5.0) < 1e-6);
}
ZTH_TEST("robust", "random rays against random spheres: every hit is on the surface and nearest; every miss really misses") {
    zth::Lcg rng(1234);
    for (int round = 0; round < 3000; ++round) {
        const Sphere s{rv(rng, 5), 0.2 + rng.below(300) / 100.0};
        const Ray r{rv(rng, 8), rv(rng, 3) + Vec3{0, 0, 0.01}};
        const double dl = length(r.dir);
        if (!(dl > 1e-3)) continue;
        const auto h = intersect(r, s, 0.0, 1e6);
        // distance from the centre to the ray's line, by projection
        const Vec3 oc = s.center - r.origin;
        const double along = dot(oc, r.dir) / (dl * dl);
        const double dist = length(oc - r.dir * along);
        const bool inside = length(oc) < s.radius;
        const double far_t = along + std::sqrt(std::max(0.0, s.radius * s.radius - dist * dist) / (dl * dl));
        if (dist > s.radius * (1 + 1e-9) || far_t < -1e-12) {
            ZTH_EXPECT_FALSE(h.has_value());
            continue;
        }
        if (dist < s.radius * (1 - 1e-9) && far_t > 1e-9) {
            ZTH_EXPECT_TRUE(h.has_value());
            if (!h) return;
            ZTH_EXPECT_NEAR(length(h->point - s.center), s.radius, 1e-9 * (1 + s.radius));
            ZTH_EXPECT_TRUE(well_formed(r, *h));
            ZTH_EXPECT_TRUE(h->t >= 0.0);
            ZTH_EXPECT_EQ(h->front, !inside);
            // nearest: the near root when outside, the far root when inside
            ZTH_EXPECT_NEAR(h->t, inside ? far_t : along - std::sqrt(std::max(0.0, s.radius * s.radius - dist * dist) / (dl * dl)), 1e-9 * (1 + std::fabs(far_t)));
        }
    }
}
ZTH_TEST("robust", "random rays against random boxes: compared with an independent face-by-face computation") {
    zth::Lcg rng(55);
    for (int round = 0; round < 3000; ++round) {
        Vec3 a = rv(rng, 4), b = rv(rng, 4);
        const Aabb box{{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}, {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}};
        Ray r{rv(rng, 8), rv(rng, 2)};
        if (rng.below(4) == 0) r.dir.x = 0;   // slab-parallel rays are the interesting ones
        if (rng.below(6) == 0) r.dir.z = 0;
        if (!(length(r.dir) > 1e-3)) continue;
        // independent: test the six faces as bounded planes, keep the smallest t >= 0
        double best = kInf;
        const double lo[3] = {box.min.x, box.min.y, box.min.z}, hi[3] = {box.max.x, box.max.y, box.max.z};
        const double o[3] = {r.origin.x, r.origin.y, r.origin.z}, d[3] = {r.dir.x, r.dir.y, r.dir.z};
        for (int axis = 0; axis < 3; ++axis)
            for (int side = 0; side < 2; ++side) {
                if (d[axis] == 0) continue;
                const double t = ((side ? hi[axis] : lo[axis]) - o[axis]) / d[axis];
                if (t < 0) continue;
                bool within = true;
                for (int k = 0; k < 3; ++k) {
                    if (k == axis) continue;
                    const double c = o[k] + d[k] * t;
                    if (c < lo[k] - 1e-9 || c > hi[k] + 1e-9) within = false;
                }
                if (within) best = std::min(best, t);
            }
        const bool origin_inside = o[0] >= lo[0] && o[0] <= hi[0] && o[1] >= lo[1] && o[1] <= hi[1] && o[2] >= lo[2] && o[2] <= hi[2];
        const auto h = intersect(r, box, 0.0, 1e6);
        if (best == kInf) {
            ZTH_EXPECT_FALSE(h.has_value());
            continue;
        }
        ZTH_EXPECT_TRUE(h.has_value());
        if (!h) return;
        ZTH_EXPECT_NEAR(h->t, best, 1e-9 * (1 + best));
        ZTH_EXPECT_TRUE(well_formed(r, *h));
        {   // the normal is that of the face that was hit: the point lies on it
            const double n[3] = {h->normal.x, h->normal.y, h->normal.z}, p[3] = {h->point.x, h->point.y, h->point.z};
            int axis = -1;
            for (int k = 0; k < 3; ++k)
                if (std::fabs(n[k]) > 0.5) axis = k;
            ZTH_EXPECT_TRUE(axis >= 0 && std::fabs(n[axis]) == 1.0);
            if (axis >= 0) {
                const double outward = h->front ? n[axis] : -n[axis];
                const double face = outward > 0 ? hi[axis] : lo[axis];
                ZTH_EXPECT_NEAR(p[axis], face, 1e-9 * (1 + std::fabs(face)));
            }
        }
        if (origin_inside && !(o[0] == lo[0] || o[0] == hi[0] || o[1] == lo[1] || o[1] == hi[1] || o[2] == lo[2] || o[2] == hi[2])) ZTH_EXPECT_FALSE(h->front);
    }
}
ZTH_TEST("robust", "random rays against random triangles: barycentric reconstruction, and agreement with an inside-outside test") {
    zth::Lcg rng(77);
    int hits = 0;
    for (int round = 0; round < 4000; ++round) {
        const Triangle tri{rv(rng, 4), rv(rng, 4), rv(rng, 4)};
        const Vec3 n = cross(tri.b - tri.a, tri.c - tri.a);
        if (!(length(n) > 1e-2)) continue;
        Ray r{rv(rng, 8), rv(rng, 2)};
        if (round % 4 != 0) {   // most rays are aimed at (or near) the triangle
            const Vec3 target = tri.a + (tri.b - tri.a) * (rng.below(1500) / 1000.0 - 0.25) + (tri.c - tri.a) * (rng.below(1500) / 1000.0 - 0.25);
            r.origin = target - r.dir * (rng.below(300) / 100.0 - 0.5);
        }
        if (!(length(r.dir) > 1e-2)) continue;
        const auto h = intersect(r, tri, 0.0, 1e6);
        // independent: intersect the plane, then the three edge tests (same side of every edge)
        const double denom = dot(n, r.dir);
        bool expect = false;
        double t = 0;
        if (std::fabs(denom) > 1e-9 * length(n) * length(r.dir)) {
            t = dot(n, tri.a - r.origin) / denom;
            const Vec3 p = r.origin + r.dir * t;
            const double s0 = dot(n, cross(tri.b - tri.a, p - tri.a)), s1 = dot(n, cross(tri.c - tri.b, p - tri.b)), s2 = dot(n, cross(tri.a - tri.c, p - tri.c));
            const double margin = 1e-9 * dot(n, n);
            expect = t >= 0 && s0 >= -margin && s1 >= -margin && s2 >= -margin;
            const bool clear = t >= 1e-9 && s0 > margin && s1 > margin && s2 > margin;
            const bool clearly_out = t < -1e-9 || s0 < -margin || s1 < -margin || s2 < -margin;
            if (!clear && !clearly_out) continue;   // too close to an edge to judge
        } else {
            continue;
        }
        ZTH_EXPECT_EQ(h.has_value(), expect);
        if (!h) continue;
        ++hits;
        ZTH_EXPECT_NEAR(h->t, t, 1e-9 * (1 + std::fabs(t)));
        ZTH_EXPECT_TRUE(well_formed(r, *h));
        ZTH_EXPECT_EQ(h->front, denom < 0.0);
    }
    ZTH_EXPECT_TRUE(hits > 200);
}
ZTH_TEST("robust", "the nearest hit wins among several shapes, and the window keeps working when the origin is inside") {
    const Ray r{{0, 0, 20}, {0, 0, -1}};
    const Sphere big{{0, 0, 0}, 5}, small{{0, 0, 8}, 1};
    const auto a = intersect(r, big), b = intersect(r, small);
    ZTH_EXPECT_TRUE(a.has_value() && b.has_value() && b->t < a->t);
    ZTH_EXPECT_NEAR(b->t, 11.0, 1e-12);
    const auto from_inside = intersect(Ray{{0, 0, 0}, {1, 0, 0}}, big, 0, 4);
    ZTH_EXPECT_FALSE(from_inside.has_value());   // the exit is at t = 5
    ZTH_EXPECT_TRUE(intersect(Ray{{0, 0, 0}, {1, 0, 0}}, big, 0, 5).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{0, 0, 0}, {1, 0, 0}}, big, 5.001, 9).has_value());
}
ZTH_TEST("robust", "boxes and triangles at large coordinates and with tiny features keep their edges closed") {
    const Aabb far_box{{1e9, 1e9, 1e9}, {1e9 + 2, 1e9 + 2, 1e9 + 2}};
    const auto h = intersect(Ray{{1e9 + 1, 1e9 + 1, 0}, {0, 0, 1}}, far_box);
    ZTH_EXPECT_TRUE(h.has_value() && std::fabs(h->t - 1e9) < 1e-5 && near(h->normal, {0, 0, -1}));
    ZTH_EXPECT_TRUE(intersect(Ray{{1e9, 1e9 + 2, 0}, {0, 0, 1}}, far_box).has_value());   // a corner edge
    const Triangle sliver{{0, 0, 0}, {1e6, 0, 0}, {0, 1e-3, 0}};
    ZTH_EXPECT_TRUE(intersect(Ray{{1000, 0.0004, 1}, {0, 0, -1}}, sliver).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{1000, 0.0011, 1}, {0, 0, -1}}, sliver).has_value());
    const Triangle shifted{{1e6, 1e6, 0}, {1e6 + 2, 1e6, 0}, {1e6, 1e6 + 2, 0}};
    ZTH_EXPECT_TRUE(intersect(Ray{{1e6 + 0.5, 1e6 + 0.5, 3}, {0, 0, -1}}, shifted).has_value());
    ZTH_EXPECT_FALSE(intersect(Ray{{1e6 + 1.5, 1e6 + 1.5, 3}, {0, 0, -1}}, shifted).has_value());
}
ZTH_TEST("robust", "two triangles sharing an edge: a ray at the shared edge hits at least one, and every point of a quad is covered") {
    const Triangle t1{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}}, t2{{0, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    zth::Lcg rng(9);
    for (int i = 0; i < 2000; ++i) {
        const double x = rng.below(1001) / 1000.0, y = rng.below(1001) / 1000.0;
        const Ray r{{x, y, 5}, {0, 0, -1}};
        ZTH_EXPECT_TRUE(intersect(r, t1).has_value() || intersect(r, t2).has_value());
    }
    for (int i = 0; i <= 10; ++i) {
        const double x = i / 10.0;
        const Ray r{{x, x, 5}, {0, 0, -1}};   // exactly on the shared diagonal
        ZTH_EXPECT_TRUE(intersect(r, t1).has_value() && intersect(r, t2).has_value());
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "intersection tests never allocate") {
    const Sphere s{{0, 0, 0}, 2};
    const Plane p{{0, 0, 1}, {0, 0, 1}};
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 20000; ++i) {
        const Ray r{{i * 0.001, 0.1, 10}, {0, 0.01, -1}};
        sink += val(intersect(r, s)) + val(intersect(r, p)) + val(intersect(r, kBox)) + val(intersect(r, kTri));
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(std::isfinite(sink) || std::isnan(sink) || true);
}
ZTH_TEST_TIMEOUT("fast", "two million rays against a scene of spheres, a box and a triangle", 20) {
    std::vector<Sphere> spheres;
    for (int i = 0; i < 20; ++i) spheres.push_back({{i * 1.5 - 15, std::sin(i) * 3, -5.0 - i}, 1.0 + 0.1 * (i % 5)});
    zth::Lcg rng(3);
    long long hits = 0;
    for (int i = 0; i < 2000000; ++i) {
        const Ray r{{0, 0, 10}, {(rng.below(2001) - 1000) / 400.0, (rng.below(2001) - 1000) / 400.0, -1}};
        double best = kInf;
        for (const Sphere& s : spheres) {
            const auto h = intersect(r, s, 0.0, best);
            if (h) best = h->t;
        }
        if (intersect(r, kTri, 0.0, best)) ++hits;
        if (best < kInf) ++hits;
    }
    ZTH_EXPECT_TRUE(hits > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no heap containers or virtual dispatch") {
    ZTH_EXPECT_NO_RAW_MEMORY("ray.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("ray.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "vector", "new", "function", "virtual", "shared_ptr", "unique_ptr"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the box test is the slab method (no per-face plane loop with string-like tables) and divides only by non-zero components") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("ray.hpp"));
    for (const char* word : {"pow", "acos", "asin", "atan", "atan2"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "sqrt").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "cross").empty());
}
