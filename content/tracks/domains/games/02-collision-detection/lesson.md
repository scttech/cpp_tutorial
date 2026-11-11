# Collision Detection

> **You're an engineer at a game studio.** The physics step takes 40 ms with 2000 objects: it tests every object against every other, 2 million pair tests per frame. You will fix it the way real engines do, in **two phases**: a cheap *broad phase* (a uniform grid) that throws away the pairs that are obviously far apart, and an exact *narrow phase* for the few that remain.

## Narrow phase: `shapes.hpp`

Three overloads of `overlaps`. Shapes that merely **touch** overlap.

- **Box vs box**: the x intervals overlap *and* the y intervals overlap.
- **Circle vs circle**: the centres are at most `r1 + r2` apart. Compare **squared** distances (`length_squared`): no square root, no rounding from `sqrt`.
- **Circle vs box**: find the point of the box **closest to the centre** (clamp the centre's coordinates to the box), then it is a point-versus-circle test. This handles sides, corners and a centre inside the box with the same code.

## Broad phase: `grid.hpp`

`UniformGrid(cell)` divides the plane into square cells. `insert(id, box)` puts the id into **every cell the box touches** (a big box spans many cells). Two objects can only collide if they share a cell, so `candidate_pairs()` just pairs up the ids within each cell. Details the tests check:

- Cell coordinates use **`floor`**, not integer truncation: `-0.5 / 10` truncates to cell 0 but belongs in cell −1.
- A box whose edge lies **exactly on a cell border** touches the cells on both sides.
- Two boxes may share several cells: report the pair **once**, sorted, with `a < b`.
- `find_collisions(boxes, cell)` runs the broad phase, then `overlaps` on each candidate, and returns the real collisions sorted.

A hash map from the packed cell coordinates to a `std::vector<int>` is a good representation (cells are created on demand, coordinates can be negative or huge).

## The tiers

- **Works**: all three overlap tests, touching, the grid with spanning boxes, `find_collisions`.
- **Robust**: negative coordinates, cell borders, point and zero-width boxes, a huge box, far-away boxes, symmetry and degenerate cases as properties, and 300 random scenes compared with brute force at four cell sizes.
- **Fast**: few candidate pairs (at most 4 per real collision plus a small allowance), 40000 boxes in time, bounded allocations, linear growth with constant density.
- **Clean**: no raw memory, no exceptions, no `sqrt`.

## Tips

Write the three `overlaps` first and run the Works tests. For the grid, write `cell_of(v) = floor(v / cell)` and loop `cy` from the min cell to the max cell, `cx` likewise. Use `std::sort` + `std::unique` to remove repeated pairs.
