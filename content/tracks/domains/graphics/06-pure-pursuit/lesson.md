# A Pure-Pursuit Path Follower

> **You're on the autonomy team.** The planner hands the robot a path: a list of waypoints. Something has to turn that into wheel commands, forty times a second, for a vehicle that cannot move sideways. **Pure pursuit** is the classic answer and it is only a few lines of geometry: pick a *carrot* on the path a fixed distance ahead, and steer along the circular arc that goes through the carrot. It drives warehouse robots, lawn mowers, agricultural tractors and the early DARPA cars, and getting its details right (never turning back along the path, handling the end, not blowing up near a corner) is what separates a demo from a product.

## The vehicle: curvature and exact arcs

A car-like or differential-drive robot moves along circular arcs: it has a speed `v` and a **curvature** `kappa` (the reciprocal of the turning radius; positive means counter-clockwise). In `dt` seconds it travels `d = v dt` along an arc turning through `delta = kappa d`. The textbook update `x += v cos(theta) dt` (Euler) *drifts off the arc* on every step; the exact update uses the chord of the arc: `chord = d * sinc(delta / 2)` in the heading `theta + delta / 2`. `sinc(u) = sin(u) / u` is `0/0` at `u = 0` (define it as 1 there). The tempting closed form `x += (sin(theta + delta) - sin(theta)) / kappa` subtracts two nearly equal numbers and is pure rounding noise for a robot driving "straight" with `kappa = 1e-12`; the chord form is accurate everywhere. Angles are kept in `(-pi, pi]` with `fmod`; a `while (a > pi) a -= 2 pi` loop never ends for `a = 1e300` and is slow for large `a`.

## The geometry

- **Circle-segment intersection.** The lookahead point is where the circle of radius `L` around the robot crosses the path. Substituting a segment into the circle equation gives a quadratic whose discriminant, computed naively as `b^2 - a c`, cancels away its digits for large coordinates (the same trap as in the ray-sphere challenge). Use `|d|^2 r^2 - cross(f, d)^2`. A tangent line is one root; a segment that only the *line* crosses has none.
- **Closest point on a segment.** Project, then clamp the parameter to `[0, 1]`. The path has no zero-length segments (that would divide by zero), which is why `Path::make` removes duplicates.
- **Choosing the carrot.** On the current segment ignore intersections the robot has already passed; scan forward and take the *first* segment that has one, at its *largest* root. Taking the "farthest intersection anywhere" would skip a hairpin; taking the nearest would stall.

## The controller

In the robot's frame (`lx` forward, `ly` left) the circle through the robot and the carrot, tangent to the robot's heading, has curvature

```
kappa = 2 * ly / (lx^2 + ly^2)
```

(for `d = L` it is the familiar `2 sin(alpha) / L`). The details that make it work in the field:

- **Monotone progress.** The follower remembers its segment and only searches ahead, a small window of segments. A path that doubles back close to itself would otherwise make it jump to the wrong leg.
- **Carrot behind the robot** (`lx < 0`): the formula gives silly values, so turn at full lock towards the carrot's side.
- **The end.** When the rest of the path is inside the circle there is no intersection: aim at the last point. Declare *done* within a tolerance and stay done.
- **Limits.** Clamp the curvature to what the vehicle can do, slow down in tight turns and near the goal, never exceed the maximum speed.
- **Garbage.** A NaN pose from a failing sensor must produce an idle command, not corrupt the state; invalid parameters must be refused up front.

## The tiers

- **Works**: angle wrapping, exact arcs, intersections, paths, projections, carrots, parameter validation, the curvature formula, the finish line.
- **Robust**: closed-loop simulations on lines, squares, circles and random paths (convergence, bounded corner cutting, limits, monotone progress), headings around `+-pi`, the hairpin, `advance` consistency and tiny curvatures, intersections far from the origin.
- **Fast**: no allocations; a million control steps on a 1000-segment path.
- **Clean**: no raw memory or exceptions; `fmod`, not a loop; no `atan2`/`acos`.
