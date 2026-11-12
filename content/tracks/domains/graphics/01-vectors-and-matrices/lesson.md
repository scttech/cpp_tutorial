# Vector and Matrix Math

> **You're on the engine team.** Everything on screen, in a game, a CAD viewer or a robot simulator, goes through the same small library: 3D vectors, 4x4 matrices, a camera, a projection. It is a few hundred lines, and a wrong sign or a swapped index in it produces a picture that is *almost* right: models mirrored, lighting inverted, a camera that flips upside down when you look straight up. You will write it carefully and test it like the foundation it is.

## Conventions are the contract

The tests (and the rest of a renderer) rely on four conventions; decide them once, write them down, never mix them:

- **Right-handed**: `x` right, `y` up, the camera looks down `-z`, and `cross(x, y) = +z`.
- **Column-major storage**: the 16 numbers are stored column after column, so element *(row r, column c)* is `m[c * 4 + r]` and a translation lives in `m[12..14]`. OpenGL, Vulkan (with GLSL) and glm all do this; DirectX/HLSL and many textbooks use row vectors and row-major.
- **Column vectors**: points are multiplied *on the right*: `p' = M * p`. So `A * B` applies **B first**. The model-view-projection matrix is `P * V * M`.
- **Points have w = 1, directions have w = 0**: a direction (a normal, a velocity) must not be translated. After a projection the result must be **divided by w**, which is where perspective foreshortening comes from.

## Camera and projection

`look_at(eye, target, up)` builds an orthonormal basis: forward `f = normalize(target - eye)`, right `s = normalize(f x up)`, true up `u = s x f`, and the matrix has `s`, `u`, `-f` as rows plus the translation `-dot(s, eye)` etc. It fails (nullopt) when `eye == target` or when you look along `up`: a lookup `normalize(f x up)` of the zero vector, the famous *gimbal flip*.

`perspective(fov, aspect, near, far)` maps the view frustum to the cube `[-1,1]^3`: `z = -near` goes to `-1` and `z = -far` to `+1`, with a **non-linear** depth in between (precision is concentrated near the camera, which is why you never set `near` to `0.0001`).

## Numbers that go wrong

- **`length` overflows.** `sqrt(x*x + y*y + z*z)` with `x = 1e200` is `sqrt(inf)`, and with `x = 1e-200` the squares are `0`. Divide by the largest component first. (`std::hypot` does this for two arguments; C++17 added a three-argument overload.)
- **`acos` for angles is a trap.** `acos(dot(a, b) / (|a| |b|))` returns `0` for any angle below ~1e-8 rad, because `cos(1e-9) == 1` in a double, and `NaN` when rounding makes the ratio `1.0000000000000002`. `atan2(|a x b|, a . b)` is accurate over the whole range.
- **Inverting a matrix can fail.** A projection or a scale by zero has no inverse. A general 4x4 inverse by cofactors divides by the determinant, so you must say what happens when it is `0` or not finite (here: `nullopt`). Real engines use cheaper inverses for rigid transforms (`transpose` for rotations, the formula for affine matrices); you test those against the general one.

## The tiers

- **Works**: vector ops, the layout, order of multiplication, rotations, determinant/inverse, `look_at`, `perspective`, refusals.
- **Robust**: `1e200` and `1e-200` vectors; tiny angles; 300 random transform chains (associativity, `det(AB)`, `(AB)^-1`); rotations as a group; random cameras and projections; `w = 0`; tiny/huge/singular inverses.
- **Fast**: nothing allocates; a million products and a hundred thousand inverses.
- **Clean**: no raw memory, exceptions, heap containers or `std::function`; `atan2`, not `acos`.
