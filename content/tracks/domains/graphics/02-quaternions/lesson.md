# Quaternions and Slerp

> **You're on the animation team.** A character's arm is at 30 degrees in one keyframe and 170 in the next, and the engine must fill in the frames between. Do it with three Euler angles and you meet *gimbal lock* (the arm suddenly loses a degree of freedom), jumps at +-180 degrees and uneven speed. Do it with a rotation matrix and the matrices drift out of orthogonality after a few thousand frames. **Unit quaternions** fix all of this: four numbers, no singularities, cheap to compose and a clean interpolation (**slerp**). Every game engine, flight simulator, drone and VR headset runs on them.

## What a quaternion is, as far as you need

A rotation by angle `a` about the unit axis `u` is the quaternion `q = (cos(a/2), sin(a/2) u)`. The *half* angle is the first thing people get wrong. Three facts do all the work:

1. **Product = composition.** `(a * b)` rotates by `b` first, then `a`: the same order as matrices, so `rotate(a * b, v) == rotate(a, rotate(b, v))`. The product does not commute.
2. **`q` and `-q` are the same rotation** (the double cover): rotating by `q` or `-q` gives the same vector. So orientations live on a sphere where opposite points are identified, and "the angle between two orientations" must take that into account.
3. **Rotating a vector** needs no matrix: `t = 2 (u x v); v' = v + w t + u x t`.

## Converting to and from a matrix

`to_matrix` is a handful of products. The inverse is where code goes wrong: the textbook `w = sqrt(1 + trace) / 2` then `x = (m21 - m12) / (4 w)` divides by a number that is **zero for a half turn** (trace = -1). **Shepperd's method** looks at the trace and the diagonal to find which of `w, x, y, z` is largest and starts from that one; the other three components then come from sums and differences of matrix entries divided by a number of at least 1. Also refuse input that is not a rotation (a scale, a shear, a mirror image): a quaternion cannot represent it.

## Slerp

Linear interpolation of the four numbers (`lerp`, then normalise: **nlerp**) takes the right path but at a varying speed: slow at the ends, fast in the middle, and visibly so for large angles. **Slerp** moves at constant angular speed:

```
theta = acos(dot(a, b))
slerp(a, b, t) = ( sin((1 - t) theta) a + sin(t theta) b ) / sin(theta)
```

Three details the tests check:

- **Shortest arc.** If `dot(a, b) < 0`, `b` and `-b` are the same orientation but `-b` is closer; interpolating toward `b` would swing the long way round.
- **Nearly equal inputs.** As `theta -> 0`, `sin(theta)` is a tiny number and the division amplifies rounding noise (or gives `0/0`). Below an angle of a few degrees switch to nlerp: the two formulas agree to many digits there.
- **`acos` of a number slightly above 1** (rounding!) is `NaN`. Clamp.

And for the angle between orientations use `2 * atan2(|vector part|, |w|)` of the relative rotation `conjugate(a) * b`: `acos` cannot resolve angles below ~1e-8 rad.

## The tiers

- **Works**: half angle, rotation about each axis, composition order, conjugate/inverse/norm, the double cover, matrix conversions, half turns, slerp endpoints, middle and shortest arc.
- **Robust**: 500 random compositions against matrices; round trips including angles within 1e-7 of 180 degrees; slerp at constant speed; stability near `theta = 0` and the switch-over; tiny angles; 1e200 quaternions; drift after 200000 products.
- **Fast**: nothing allocates; a million slerps.
- **Clean**: no raw memory, exceptions or heap containers; `rotate` does not go through a matrix; `atan2` and `hypot` are used.
