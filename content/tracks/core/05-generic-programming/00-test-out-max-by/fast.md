# Fast path: what's different in C++

- A **function template** is a recipe: `template <class T> ...` and the compiler writes the real function for each set of types you use (**instantiation**), deducing `T` from the arguments.
- `Range&&` with `Range` deduced is a **forwarding reference**: it binds to lvalues and rvalues, keeps `const`. Use `std::begin(r)` / `std::end(r)`: they work for containers and C arrays.
- **`std::invoke(f, args...)`** calls any callable, including a **member pointer** (`&Person::age`). A projection is a callable that turns an element into a key.
- A template parameter can have a **default** (`class Compare = std::less<>`); a transparent comparator `std::less<>` compares any two types with `<`.
- `auto` deduces by value: `auto key = f(x);` stores a copy of the key (what you want when remembering the best one).
- Remember the best key; don't recompute it each iteration (it doubles the projection calls).
- Only require what you use: comparing with `comp(a, b)` needs nothing from the key type except what `comp` needs.
