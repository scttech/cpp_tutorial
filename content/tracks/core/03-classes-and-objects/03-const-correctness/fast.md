# Fast path: what's different in C++

- **`const` after the parameter list = "does not modify the object".** A `const T&` can only call const member functions, so a missing `const` on a getter breaks every function that takes a `const&`. Make everything that only reads `const`.
- **Const overloads:** `T& at(i)` and `const T& at(i) const`. The compiler picks by the object's constness. Never `const_cast` to return a mutable reference from the const one.
- **Parameters:** read-only and not cheap to copy -> `const T&`. By value copies; a non-const `T&` rejects const objects and temporaries.
- **`mutable`** is for hidden implementation detail (cache, counter, mutex) that a `const` function may update. A cache needs invalidation: anything that can change the data (including the non-const accessor that hands out a reference) must reset it.
- A function **defined in a header** must be `inline` (or `static`/in a class), or two includes mean two definitions.
