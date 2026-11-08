# Fast path: what's different in C++

- A **template** (`template <class T> ...`) is a recipe; the compiler **instantiates** a real function per set of types used. Templates live in **headers**.
- **Deduction**: `T` comes from the arguments, and every use must agree (`clamp_value(5, 0, 3.5)` fails: `int` vs `double`). Fix by unifying the types or naming `T`: `clamp_value<double>(5, 0, 3.5)`. Separate type parameters (`A`, `B`) when types may differ; the result is `std::common_type_t<A, B>`.
- `std::initializer_list<T>` parameter deduces `T` from `{...}`.
- **Constrain by use, not by name**: ask of `T` only what you need (here just `<`), so the template works for the widest range of types.
- **ADL swap idiom**: `using std::swap; swap(a, b);`. Unqualified call, so a type's own `swap` (found by argument-dependent lookup) is used, with `std::swap` as the fallback. Never `std::swap(a, b)` in generic code.
- `const T&` params + `const T&` result: no copies, but the result refers to an argument.
- Template errors appear at the instantiation: read the notes ("in instantiation of ... requested here") to find which type and which call.
