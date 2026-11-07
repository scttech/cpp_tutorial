# Fast path: what's different in C++

- A C array `int a[4]` has no bounds checking, cannot be copied or returned, and **decays to a pointer** when passed to a function. `sizeof(param)` inside the function is the pointer's size, so pass a length alongside.
- `std::array<T, N>` is the same storage with value semantics: copy, return, `==`, `.size()`. `operator[]` is still unchecked; `.at()` checks (and throws).
- Indexes are `std::size_t` (unsigned): `-1` becomes a huge value, so a single `index >= size` check catches both ends.
- Summing ints: accumulate in `long long`, because `int` overflow is undefined behavior.
- Nested `std::array<std::array<int, C>, R>` is an R x C grid, indexed `[row][col]`.
