# Fast path: what's different in C++

- **Use `enum class`** (scoped: `Status::Paid`; no implicit conversion to `int`; names do not leak). Fix the underlying type when it matters: `enum class Status : std::uint8_t { ... }`. Convert deliberately with `static_cast<int>`.
- **`switch` over an enum with every case and no `default:`**: the compiler then warns when a new enumerator is added and not handled (`-Wswitch`). Add a `return` after the switch for values outside the enumerators.
- **`static` data member** = one copy shared by the class. In a header write **`inline static int n = 0;`** (C++17); a plain `static int n;` also needs a definition in one .cpp. **`static constexpr`** for class-wide constants.
- **`static` member function**: no `this`, called as `Class::f()`; cannot touch non-static members.
- A per-object counter (non-static) is the classic bug: every object counts from zero.
- State machines: put the transition table in **one function** (`allowed(from, to)`); every action asks it.
