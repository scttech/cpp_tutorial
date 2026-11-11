# Fast path: what's different in C++

- **Compile-time evaluation is a language feature**, not a build-system trick: `constexpr` (may), `consteval` (must), `constinit` (initialize statically), `static_assert` (check). Since C++20 much of the standard library is `constexpr` (algorithms, `std::array`, `std::vector`/`std::string` inside a constant expression, `std::bit_cast`).
- **Undefined behavior is an error in a constant expression**: overflow, out-of-bounds, null dereference. That makes `constexpr` a cheap way to test code.
- **`if consteval`** (C++23) and `std::is_constant_evaluated()` (C++20) let one function take different paths at compile time and at run time.
- **Templates are the older tool**: `std::array<int, N>` and `template <int N>` take compile-time values, and `if constexpr` chooses code at compile time. Prefer `constexpr` functions to template metaprogramming for computing values.
- **Cost model**: nothing at run time, but compile time and the interpreter's step limit (`-fconstexpr-steps=`, `-fconstexpr-depth=`). Put big tables in one translation unit (`inline constexpr` in a header is built once per file that includes it, so a build can slow down).
