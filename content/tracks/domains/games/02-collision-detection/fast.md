# Fast path: what's different in C++

- **Two phases, like every engine.** Broad phase (grid, quadtree, sweep-and-prune, BVH) prunes; narrow phase (GJK, SAT, these closed-form tests) decides. Box2D, Bullet, PhysX and Unity all separate them.
- **`std::clamp`** (C++17) is the closest-point trick in one line; it is also why the same code handles sides and corners.
- **Squared distances.** `sqrt` is slow and inexact; comparing `d² <= r²` avoids both. Many engines never take a square root in the collision path.
- **Floating-point equality at boundaries.** "Touching" depends on exact comparisons (`<=`), so the tests use values that are exactly representable (multiples of 0.5, 3-4-5 triangles). Real engines add a small epsilon (skin width) for stability.
- **Hashing cells.** `std::unordered_map` with a packed 64-bit key is a *spatial hash*: unbounded worlds, cells created on demand. Packing two signed 32-bit coordinates needs care: cast to unsigned before shifting (shifting a negative signed value is a sign trap).
- **`std::floor` vs `static_cast<int>`**: conversion truncates toward zero, which maps -0.5 and +0.5 to the same cell 0. Negative coordinates are the classic grid bug.
