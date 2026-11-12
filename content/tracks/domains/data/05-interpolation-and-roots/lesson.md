# Interpolation and Root Finding

> **You're on the numerics team.** Sensors report at irregular times and a model needs values on a regular grid: that is *interpolation*. A pricing model asks "at what rate does the present value equal the price?", and a control loop asks "at what throttle does the error cross zero?": that is *root finding*. Both are a page of code in a textbook and a source of production incidents: a lookup that scans a million knots linearly, a Newton iteration that wanders off to infinity, a "root" that is really a pole.

## Interpolation

**Piecewise linear** is the workhorse: find the segment `[x_i, x_(i+1)]` that holds `x` (a **binary search**: `std::upper_bound` on the sorted knots; a linear scan makes a million-point table a million times slower) and blend the two ends. Three details:

- Write the blend as `y0 * (1 - t) + y1 * t`. The textbook `y0 + t * (y1 - y0)` overflows when the ends are `-1.7e308` and `1.7e308`.
- At a knot you must get the table's own value back; the last knot belongs to the *last* segment (so `upper_bound` must be clamped).
- Outside the table there is no answer. Extrapolating silently is how a thermometer table gives `-4000` degrees.

A **natural cubic spline** passes through the same points but is smooth: the slope and the curvature are continuous at every knot, and the curvature is zero at both ends. The unknowns are the second derivatives `m_i`; the conditions give a **tridiagonal** system you solve in `O(n)` with the Thomas algorithm (a forward sweep that eliminates the lower diagonal, a back substitution). No matrix, no library.

## Root finding

| Method | Needs | Convergence | Fails when |
|---|---|---|---|
| **Bisection** | a sign change `f(a) f(b) < 0` | halves the bracket: `log2((b - a) / tol)` steps | the "sign change" is a **pole** (`1/x`, `tan`) |
| **Newton** | `f'` and a start near the root | doubles the correct digits per step | `f' = 0`; a cycle (`x^3 - 2x + 2` from 0); divergence (`atan` from 1.5) |
| **Safeguarded Newton** | a bracket and `f'` | Newton's speed, bisection's guarantee | almost never |

Safeguarded Newton (Numerical Recipes' `rtsafe`) keeps the bracket, takes the Newton step only when it stays inside the bracket *and* is at least halving the previous step, and bisects otherwise. It is what you reach for when you must not fail.

The traps the tests probe:

- `(a + b) / 2` overflows; use `std::midpoint`.
- `tol == 0` must still terminate: stop when the midpoint equals an end, because the interval cannot shrink any further.
- A function returning NaN or infinity has no sign: refuse the call instead of looping.
- A sign change at a pole is not a root. After converging, check that `|f|` at the answer is not bigger than at the ends.
- A zero (or NaN) derivative must not crash Newton or stall the safeguarded version.

Callables are **template parameters**, not `std::function`: the compiler can inline `f` into the loop, and you pay nothing for the generality.

## The tiers

- **Works**: tables, refusals, the spline against exact rational arithmetic, all three solvers, their refusals, `max_iter` and the tolerance.
- **Robust**: random tables against a brute-force scan; values near `1e308`; spline exactness on lines and continuity of value and slope; spline versus polyline accuracy on `sin`; 300 random bisections; `tol = 0`; flat tails, a triple root, Newton's cycle.
- **Fast**: nothing allocates; a spline is built with a handful of arrays; function-call counts for every solver; a million-knot table queried a million times.
- **Clean**: no raw memory or exceptions; no `std::function`, no linear `find`; a binary search is present.
