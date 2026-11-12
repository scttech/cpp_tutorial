# Ray Intersection

> **You're on the rendering team.** A ray tracer, a physics engine's raycast, a 3D editor's mouse picking, a lidar simulator and a shooter's hit detection all ask the same question millions of times per frame: *where does this ray first touch that shape?* The formulas fit on an index card, and the bugs hide in the edge cases: the ray that starts inside the sphere, the ray exactly parallel to the plane, the box face a ray grazes, the object 100 million units away. Each of those is a pixel of speckle or a bullet that goes through a wall.

## The hit record

A ray is `origin + t * dir`. Keep `dir` as given (not necessarily unit): then `t` is a *time* in units of the direction, which is what you want for motion. A hit has `t`, the `point`, and a **normal that faces the ray** with a `front` flag. Shading and reflection want the normal pointing at the viewer; the flag says whether you came from outside (`front`) or from inside (refraction needs to know). Every intersection function ends in the same helper that decides both from `dot(dir, outward_normal)`.

The query is over a **closed window** `[t_min, t_max]`: "the nearest hit that is at least `t_min` and at most `t_max`". Shadow rays use a small `t_min` to avoid hitting their own origin surface; a scene search shrinks `t_max` to the best hit so far so that later objects can be rejected early. With a window, the *far* root of a sphere can be the answer (when the near root is before `t_min`).

## Sphere, and why the textbook formula fails

Substitute the ray into `|p - c|^2 = r^2` and you get a quadratic with discriminant `b^2 - a c` where `c = |oc|^2 - r^2`. For a sphere 1e8 away, `|oc|^2` is 1e16 and `r^2` is 1: **the subtraction has no digits left**, and a ray right through the middle reports a miss. Lagrange's identity rewrites the discriminant as `a r^2 - |oc x dir|^2`: the cross product is small when the ray is aimed at the sphere, nothing large is subtracted, and the answer is accurate at any distance.

## Plane, box, triangle

- **Plane**: `t = n . (p0 - o) / (n . d)`. A ray parallel to the plane (`n . d == 0`) never hits, *including a ray lying in the plane*: there is no single point to report.
- **Box (slab method)**: the box is the intersection of three slabs. For each axis compute the `t` where the ray enters and leaves the slab, and keep the largest entry and the smallest exit. If `t_enter > t_exit` it is a miss. The trap is an axis with `d == 0`: `1/d` is infinity and `(lo - o) * inf` is NaN when `lo == o`, and NaN silently wrecks the `min`/`max` formulations that fast tracers use. Give that axis its own branch: miss unless the origin is inside the slab (inclusive), otherwise skip it. (With plain `if` comparisons a NaN is simply ignored, which happens to give the right answer; do not rely on it.)
- **Triangle (Moller-Trumbore)**: solve for the barycentric coordinates `u, v` and `t` directly with two cross products and a determinant; `u >= 0, v >= 0, u + v <= 1` means inside. Keep the edges **closed**: with open edges a ray through the shared edge of two triangles can slip between them ("pixel cracks" in a mesh). A determinant near zero means the ray is parallel to the triangle.

## Refuse the impossible

Zero or NaN directions, a radius of 0, `min > max`, a triangle with no area, `t_min > t_max`: return nullopt instead of NaN-poisoned output. The hit contract is a promise to code that will normalise the normal and shade with it.

## The tiers

- **Works**: every shape's basic hit, miss, tangent, inside, parallel, edges and corners; the window; non-unit directions; front/back faces; invalid input.
- **Robust**: a sphere at 1e8; thousands of random rays checked against independent computations (distance to the line for spheres, face by face for boxes, inside-outside tests for triangles); large coordinates; slivers; shared edges.
- **Fast**: no allocations; two million rays against a scene.
- **Clean**: no raw memory, exceptions, heap containers; no trigonometry.
