# Fast path: what's different in C++

- **A lambda is an unnamed class**: captures are its data members, the body is `operator()`, `const` unless the lambda is `mutable`. Every lambda has its own type (two identical lambdas have different types).
- **Captures**: `[x]` by value (a copy, immutable unless `mutable`), `[&x]` by reference (no ownership), `[x = std::move(y)]` init-capture (a new member; the only way to capture move-only things or to compute a value), `[=]`/`[&]` capture everything used (and `[this]` captures the pointer, not the object: it dangles if the object dies).
- **A closure that outlives its scope must capture by value.** By-reference captures of locals (including loop variables) dangle.
- **`std::function<R(A...)>`**: type-erased, copyable, may heap allocate, indirect call. Copying it copies the callable. Requires the callable to be **copyable** (use `std::move_only_function` in C++23 for move-only ones).
- **Take callables as templates** (`template <class F> void f(F&& fn)`) when you only call them; store them as `std::function` only when you need to keep heterogeneous callables.
- **Generic lambdas** (`[](auto&& x)`) have a templated call operator; forward with `std::forward<decltype(x)>(x)`.
- **`std::invoke(f, args...)`** also calls member function pointers and data member pointers (`std::invoke(&Person::name, p)`).
- **Order matters in a registry**: handlers called in registration order; removing one must not disturb the order of the others, and ids are never reused.
