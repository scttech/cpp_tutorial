# Lines and Triangles: Rasterization

> **You're on the graphics team.** Before a GPU shades a pixel, a rasterizer decides *which pixels a shape covers*. Every GPU does it in fixed-point integer arithmetic for a reason: floating point makes two triangles that share an edge disagree about the pixels on it, and the picture shows cracks or doubly-blended seams. Software renderers, emulators, font engines and plotters still do it by hand. You will write the two core primitives, a **line** and a **filled triangle**, with exact, deterministic rules.

## Lines: one formula per pixel

Bresenham's algorithm walks along the *major* axis (the one with the larger difference) one pixel at a time and decides, with an integer error term, when to step in the minor axis. The pixel it picks is the one nearest the ideal line. Written as a formula, for `k = 0 .. DX`:

```
y = y0 + sy * ((2 * k * DY + DX) / (2 * DX))        // integer division: round half up
```

Having a formula matters because it lets you do three things the incremental version makes awkward:

1. **Clip without changing the line.** Cutting the line at the canvas border with floating-point geometry moves where the pixels fall (the slope is rounded differently). Instead keep the *same* line and only visit the `k` whose major coordinate is on the canvas: the cost follows the canvas, not the line (the tests draw lines from -5e8 to +5e8).
2. **Be symmetric.** `draw(a, b)` and `draw(b, a)` must give the same pixels (otherwise a polygon outline has seams depending on vertex order): sort the endpoints along the major axis first.
3. **Be testable.** The tests compare against the formula, and against an *incremental* Bresenham for the same rule.

Mind the limits: `2 * k * DY` reaches `2^61` for coordinates near `2^29`. That is why the coordinates are limited, and why the function reports `false` for anything larger instead of overflowing (signed overflow is undefined behaviour).

## Triangles: edge functions

For an edge `a -> b` and a point `p`, `w = (b - a) x (p - a)` is positive on one side of the edge, zero on it, and negative on the other. A point is inside a triangle when all three edge functions agree in sign. That is a few multiplications per pixel and needs no division, which is why hardware does it that way, evaluating a whole block of pixels in parallel.

- **Pixel centres.** Sample at the centre `(x + 0.5, y + 0.5)`. With 28.4 fixed point (16 sub-pixel units per pixel) that is `16 x + 8`.
- **Winding.** Compute the signed `area2`; swap two vertices when it is negative so any winding works; zero means degenerate.
- **The top-left rule.** What if a centre lies *exactly* on an edge (`w == 0`)? If both neighbours of a shared edge draw it you get a doubled pixel (a bright seam when blending); if neither does you get a hole. The fix: an edge *owns* its boundary pixels depending on its direction, chosen so that for the two triangles that share the edge, which traverse it in opposite directions, exactly one owns it. (Direct3D and OpenGL call this the top-left rule.)
- **Bounding box.** Do not test every pixel on the screen. Convert the vertex bounds to pixel indices, clip them to the canvas, and loop over that. Use a **floor** division for the conversion: `-1 / 16` is `0` in C++ (it truncates), but the pixel containing coordinate `-1` is `-1`. (Because the box is then clamped to the canvas, a truncating division only makes the box a little larger than needed; but the habit matters the moment you drop the clamp.)

## What the tests look for

The Robust tier compares thousands of random shapes pixel by pixel with an independent coverage function, including shapes thousands of times bigger than the canvas, vertices exactly on pixel centres and corners (every pixel a tie), and quads and fans of triangles that must cover each pixel **exactly once** (the canvas counts its writes). The Fast tier draws 200000 enormous lines and hundreds of enormous triangles; the Clean tier bans floating point altogether.

## The tiers

- **Works**: lines in all octants and directions, the tie rule, clipping, refusals, triangle coverage, windings, degenerate input.
- **Robust**: random lines and triangles against the definition; ties; watertight quads and fans; negative coordinates.
- **Fast**: no allocation; 200000 enormous lines, huge triangles, full-screen fills.
- **Clean**: integer-only; no raw memory, exceptions or heap containers.
